/*
 *  hpc-workspace-v2
 *
 *  ws_stat
 *
 *  - tool to get statistics about workspaces (bshoshany/thread-pool version)
 *  - within a workspace directory, parallel processing with a lock free work stealing queue
 *
 *  relies on statx(), not available in RH7 and older.
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *
 *  (c) Holger Berger 2021,2023,2024,2025,2026
 *  (c) Christoph Niethammer 2025
 *
 *  hpc-workspace-v2 is based on workspace by Holger Berger, Thomas Beisel and Martin Hecht
 *
 *  hpc-workspace-v2 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  hpc-workspace-v2 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with workspace-ng  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

// Include bshoshany thread-pool
#define BS_THREAD_POOL_NATIVE_EXTENSIONS
#include "BS_thread_pool.hpp"

// Include work-stealing queue for paths
#include "PathWorkStealingQueue.hpp"

#include "config.h"
#include <boost/program_options.hpp>

#include "build_info.h"
#include "db.h"
#include "fmt/format.h"  // IWYU pragma: keep
#include "fmt/ostream.h" // IWYU pragma: keep
#include "fmt/ranges.h"  // IWYU pragma: keep

#include "user.h"
#include "utils.h"

#include "caps.h"
#include "ws.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <syslog.h>

#include "spdlog/spdlog.h"

// init caps here, when euid!=uid
Cap caps{};

namespace po = boost::program_options;
namespace cppfs = std::filesystem;

using namespace std;

bool debugflag = false;
bool traceflag = false;
bool verbose = false;
int debuglevel = 0;
unsigned int thread_count = 0; // 0 = default (hardware_concurrency)

// result type for stat collection
struct StatResult {
    uint64_t files{0};
    uint64_t softlinks{0};
    uint64_t directories{0};
    uint64_t bytes{0};
    uint64_t blocks{0};

    StatResult& operator+=(const StatResult& other) {
        files += other.files;
        softlinks += other.softlinks;
        directories += other.directories;
        bytes += other.bytes;
        blocks += other.blocks;
        return *this;
    }
};

// return filesize using statx(), this works only from redhat >= 8
std::pair<uint64_t, uint64_t> getfilesize(const cppfs::path& path) {
    struct statx statxbuf;

    int mask = STATX_SIZE | STATX_BLOCKS;
    int flags = AT_STATX_DONT_SYNC | AT_SYMLINK_NOFOLLOW;

    int ret = statx(0, path.c_str(), flags, mask, &statxbuf);

    if (ret == 0) {
        return {statxbuf.stx_size, statxbuf.stx_blocks};
    } else {
        return {0, 0};
    }
}

// Mutex for synchronizing output from multiple threads
std::mutex output_mutex;

// Collect only files and subdirectories at current level (no recursion)
void collect_level(const cppfs::path& path, StatResult& result, std::vector<cppfs::path>& subdirs) {
    if (!cppfs::is_directory(path)) {
        return;
    }

    try {
        for (const auto& entry : cppfs::directory_iterator(path)) {
            if (entry.is_symlink()) {
                result.softlinks++;
            } else if (entry.is_directory()) {
                result.directories++;
                subdirs.push_back(entry.path());
            } else if (entry.is_regular_file()) {
                try {
                    auto [bytes, blocks] = getfilesize(entry.path());
                    result.files++;
                    result.bytes += bytes;
                    result.blocks += blocks;
                } catch (const std::exception& e) {
                    spdlog::warn("Cannot stat file {}: {} - skipping", entry.path().string(), e.what());
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("Error reading directory {}: {}", path.string(), e.what());
    }
}

// Worker thread using per-queue work-stealing
void worker_thread(size_t worker_id, std::vector<StatResult>& local_results,
                   std::vector<PathWorkStealingQueue<cppfs::path>>& worker_queues, std::atomic<int64_t>& work_counter) {
    StatResult& local = local_results[worker_id];

    while (true) {
        // Try to pop from own queue
        std::optional<cppfs::path> work = worker_queues[worker_id].pop();

        // If own queue empty, try to steal from others in batches (fair round-robin)
        if (!work.has_value()) {
            static thread_local std::vector<cppfs::path> stolen_batch;
            if (stolen_batch.capacity() < 16) {
                stolen_batch.reserve(16);
            }

            for (size_t i = 1; i < worker_queues.size(); i++) {
                size_t victim = (worker_id + i) % worker_queues.size();
                stolen_batch.clear();
                if (worker_queues[victim].steal(stolen_batch, 16) > 0) {
                    work = std::move(stolen_batch.back());
                    stolen_batch.pop_back();

                    // Push remaining stolen items to own queue for future pop() calls
                    for (auto& p : stolen_batch) {
                        worker_queues[worker_id].push(std::move(p));
                    }
                    break;
                }
            }
        }

        // Termination check: own queue empty, no work to steal, counter is 0
        if (!work.has_value() && work_counter.load(std::memory_order_acquire) == 0) {
            break;
        }

        if (!work.has_value()) {
            std::this_thread::yield();
            continue;
        }

        // Use RAII guard to ensure the work_counter is decremented even if an exception occurs
        struct WorkGuard {
            std::atomic<int64_t>& cnt;
            explicit WorkGuard(std::atomic<int64_t>& c) : cnt(c) {}
            ~WorkGuard() { cnt.fetch_sub(1, std::memory_order_acq_rel); }
        } guard(work_counter);

        // Process this directory level
        try {
            if (cppfs::exists(*work) && cppfs::is_directory(*work)) {
                for (const auto& entry : cppfs::directory_iterator(*work)) {
                    if (entry.is_symlink()) {
                        local.softlinks++;
                    } else if (entry.is_directory()) {
                        local.directories++;
                        try {
                            cppfs::path p = entry.path();
                            work_counter.fetch_add(1, std::memory_order_acq_rel);
                            worker_queues[worker_id].push(std::move(p));
                        } catch (const std::exception& e) {
                            spdlog::warn("Cannot process subdirectory in {}: {}", work->string(), e.what());
                        }
                    } else if (entry.is_regular_file()) {
                        try {
                            auto [bytes, blocks] = getfilesize(entry.path());
                            local.files++;
                            local.bytes += bytes;
                            local.blocks += blocks;
                        } catch (const std::exception& e) {
                            spdlog::warn("Cannot stat file {}: {} - skipping", entry.path().string(), e.what());
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::warn("Error reading directory {}: {} - skipping", work->string(), e.what());
        }
    }
}

// Work-stealing based directory traversal
StatResult stat_workspace(const std::string& wspath, unsigned int override_workers = 0) {
    if (!cppfs::is_directory(wspath)) {
        spdlog::error("workspace <{}> does not exist!", wspath);
        return StatResult{};
    }

    StatResult result{};
    std::vector<cppfs::path> subdirs;

    try {
        collect_level(wspath, result, subdirs);
    } catch (const std::exception& e) {
        spdlog::error("Cannot read workspace root {}: {}", wspath, e.what());
        return result;
    }

    if (subdirs.empty()) {
        return result;
    }

    size_t worker_count = (override_workers > 0) ? override_workers : thread_count;
    if (debugflag) {
        spdlog::debug("Using {} workers for directory traversal (thread_count={})", worker_count, thread_count);
    }

    // Create one queue per worker
    std::vector<PathWorkStealingQueue<cppfs::path>> worker_queues(worker_count);

    // Round-robin distribute initial work
    for (size_t i = 0; i < subdirs.size(); i++) {
        worker_queues[i % worker_count].push(std::move(subdirs[i]));
    }

    // Work counter for termination detection
    std::atomic<int64_t> work_counter(static_cast<int64_t>(subdirs.size()));

    std::vector<StatResult> local_results(worker_count);
    std::vector<std::thread> workers;

    // Spawn workers with their own queues
    for (size_t i = 0; i < worker_count; i++) {
        workers.emplace_back(worker_thread, i, std::ref(local_results), std::ref(worker_queues),
                             std::ref(work_counter));
    }

    // Wait for all workers to complete
    for (auto& w : workers) {
        w.join();
    }

    // Aggregate results
    for (const auto& lr : local_results) {
        result += lr;
    }

    return result;
}

// helper for fmt:: formatter for boost program_options
template <> struct fmt::formatter<po::options_description> : ostream_formatter {};

//
// main logic here
//
int main(int argc, char** argv) {

    // options and flags
    string filesystem;
    string user;
    string configfile;
    string pattern;
    bool listgroups = false;

    bool sortbyname = false;
    bool sortbycreation = false;
    bool sortbyremaining = false;
    bool sortreverted = false;

    po::variables_map opts;

    // locals settings to prevent strange effects
    utils::setCLocal();

    // set custom logging format
    utils::setupLogging(string(argv[0]));

    // define options
    po::options_description cmd_options("\nOptions");
    // clang-format off
    cmd_options.add_options()
        ("help,h", "produce help message")
        ("version,V", "show version")
        ("filesystem,F", po::value<string>(&filesystem), "filesystem to list workspaces from")
        ("username,u", po::value<string>(&user), "only show workspaces for selected user")
        ("groupname,g", "enable listing of group workspaces")
        ("config", po::value<string>(&configfile), "config file")
        ("pattern,p", po::value<string>(&pattern), "pattern matching name (glob syntax)")
        ("name,N", "sort by name")
        ("creation,C", "sort by creation date")
        ("remaining,R", "sort by remaining time")
        ("reverted,r", "revert sort")
        ("verbose,v", "verbose listing")
        ("threads,t", po::value<unsigned int>(&thread_count)->default_value(0), "threads for parallel operation (default: hardware_concurrency)");
    // clang-format on

    po::options_description secret_options("Secret");
    secret_options.add_options()("debug", "show debugging information")("trace", "show tracing information");

    // define options without names
    po::positional_options_description p;
    p.add("pattern", 1);

    po::options_description all_options;
    all_options.add(cmd_options).add(secret_options);

    // parse commandline
    try {
        po::store(po::command_line_parser(argc, argv).options(all_options).positional(p).run(), opts);
        po::notify(opts);
    } catch (...) {
        fmt::print(stderr, "Usage: {} [options] [pattern]\n", argv[0]);
        fmt::println(stderr, "{}", cmd_options);
        exit(1);
    }

    // get flags

    listgroups = opts.count("group");
    verbose = opts.count("verbose");
    sortbyname = opts.count("name");
    sortbycreation = opts.count("creation");
    sortbyremaining = opts.count("remaining");
    sortreverted = opts.count("reverted");

    // global flags
    debugflag = opts.count("debug");
    traceflag = opts.count("trace");

    // thread count: CLI option overrides WS_THREADS env variable, which overrides default
    if (thread_count == 0) {
        const char* env_threads = std::getenv("WS_THREADS");
        if (env_threads != nullptr && std::string(env_threads) != "") {
            try {
                thread_count = std::stoul(env_threads);
                if (thread_count == 0)
                    thread_count = 1;
                if (debugflag) {
                    spdlog::debug("Using WS_THREADS={} from environment", thread_count);
                }
            } catch (...) {
                spdlog::warn("Invalid WS_THREADS value '{}', using default", env_threads);
                thread_count = std::thread::hardware_concurrency();
                if (thread_count == 0)
                    thread_count = 1;
            }
        } else {
            thread_count = std::thread::hardware_concurrency();
            if (thread_count == 0)
                thread_count = 1; // fallback if hardware_concurrency fails
        }
    }

    // Apply minimum thread count for effective parallelism
    if (thread_count < 4) {
        if (debugflag) {
            spdlog::debug("thread count {} too low, increasing to 4", thread_count);
        }
        thread_count = 4;
    }

    // handle options exiting here

    if (opts.count("help")) {
        fmt::print(stderr, "Usage: {} [options] [pattern]\n", argv[0]);
        fmt::println(stderr, "{}", cmd_options);
        exit(0);
    }

    if (opts.count("version")) {
        utils::printVersion("ws_stat");
        utils::printBuildFlags();
        exit(0);
    }

    // read config
    //   user can change this if no setuid installation OR if root
    auto configfilestoread = std::vector<cppfs::path>{"/etc/ws.d", "/etc/ws.conf"};
    if (configfile != "") {
        if (user::isRoot() || caps.isUserMode()) {
            configfilestoread = {configfile};
        } else {
            spdlog::warn("ignored config file options!");
        }
    }

    auto config = Config(configfilestoread);
    if (!config.isValid()) {
        spdlog::error("No valid config file found!");
        exit(-2);
    }

    // check if user is in debugusers list
    if (!config.isDebugUser(user::getUsername())) {
        if (debugflag || traceflag) {
            spdlog::warn("debug mode disabled, not in debugusers list");
            debugflag = false;
            traceflag = false;
        }
    }

    // root and admins can choose usernames
    string username = user::getUsername(); // used for rights checks
    string userpattern;                    // used for pattern matching in DB
    if (user::isRoot() || config.isAdmin(user::getUsername())) {
        if (user != "") {
            userpattern = user;
        } else {
            userpattern = "*";
        }
    } else {
        userpattern = username;
    }

    // list of groups of this process
    auto grouplist = user::getGrouplist();

    // if not pattern, show all entries
    if (pattern == "")
        pattern = "*";

    // where to list from?
    vector<string> fslist;
    vector<string> validfs = config.validFilesystems(username, grouplist, ws::LIST);
    if (filesystem != "") {
        if (canFind(validfs, filesystem)) {
            fslist.push_back(filesystem);
        } else {
            spdlog::error("invalid filesystem given.");
        }
    } else {
        fslist = validfs;
    }

    vector<std::unique_ptr<DBEntry>> entrylist;

    // iterate over filesystems
    for (auto const& fs : fslist) {
        if (debugflag)
            spdlog::debug("loop over fslist {} in {}", fs, fslist);

        std::unique_ptr<Database> db;
        try {
            db = std::unique_ptr<Database>(config.openDB(fs));
        } catch (DatabaseException& e) {
            spdlog::error(e.what());
            continue;
        }

        // Process database entries serially to avoid nested parallelism issues
        std::mutex mtx;
        auto matches = db->matchPattern(pattern, userpattern, grouplist, false, listgroups);
        for (size_t i = 0; i < matches.size(); i++) {
            try {
                auto entry = db->readEntry(matches[i], false);
                // if entry is valid
                if (entry) {
                    lock_guard<mutex> lock(mtx);
                    entrylist.push_back(std::move(entry));
                }
            } catch (DatabaseException& e) {
                spdlog::error(e.what());
            }
        }

    } // loop over fs

    if (sortbyremaining) {
        std::sort(entrylist.begin(), entrylist.end(),
                  [](const auto& x, const auto& y) { return (x->getRemaining() < y->getRemaining()); });
    } else if (sortbycreation) {
        std::sort(entrylist.begin(), entrylist.end(),
                  [](const auto& x, const auto& y) { return (x->getCreation() < y->getCreation()); });
    } else if (sortbyname) {
        std::sort(entrylist.begin(), entrylist.end(),
                  [](const auto& x, const auto& y) { return (x->getId() < y->getId()); });
    }

    if (sortreverted) {
        std::reverse(entrylist.begin(), entrylist.end());
    }

    bool sort = sortbyname || sortbycreation || sortbyremaining;

    std::locale::global(std::locale("en_US.UTF-8"));

    // Calculate sqrt for nested parallelism: sqrt(thread_count) per level
    // This ensures total threads ~= thread_count when processing multiple workspaces
    unsigned int n = static_cast<unsigned int>(std::sqrt(thread_count));
    if (n < 2)
        n = 2; // minimum for effective parallelism
    if (n > thread_count)
        n = thread_count;

    // Create thread pool for workspace-level parallelism
    BS::thread_pool workspace_pool(n);
    if (debugflag) {
        spdlog::debug("Creating workspace_pool with {} threads (sqrt of total {})", n, thread_count);
    }

    // Process workspaces
    if (!sort && entrylist.size() > 1) {
        if (debugflag) {
            spdlog::debug("Parallel workspace processing with {} workspaces", entrylist.size());
        }
        workspace_pool
            .submit_loop(0, entrylist.size(),
                         [&entrylist, &username, n](size_t i) {
                             auto begin = std::chrono::steady_clock::now();
                             auto result = stat_workspace(entrylist[i]->getWSPath(), n);
                             auto end = std::chrono::steady_clock::now();
                             auto secs = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

                             // Use mutex to prevent interleaved output from parallel threads
                             std::lock_guard<std::mutex> lock(output_mutex);
                             syslog(LOG_INFO, "stat for user <%s> for workspace <%s> (%ld msec, %ld files)",
                                    username.c_str(), entrylist[i]->getId().c_str(), secs, result.files);
                             fmt::println("Id: {}", entrylist[i]->getId());
                             fmt::println("    workspace directory : {} ", entrylist[i]->getWSPath());
                             fmt::println("    files               : {}\n"
                                          "    softlinks           : {}\n"
                                          "    directories         : {}\n"
                                          "    bytes               : {:L} ({})\n"
                                          "    blocks              : {:L}",
                                          result.files, result.softlinks, result.directories, result.bytes,
                                          utils::prettyBytes(result.bytes), result.blocks);
                             if (verbose) {
                                 fmt::println("\n    time[msec]          : {}", secs);
                                 if (secs > 0) {
                                     fmt::println("    KFiles/sec          : {}", (double)result.files / secs);
                                 }
                             }
                         })
            .wait();
    } else {
        // Serial processing when sorted or for small lists
        for (auto& entry : entrylist) {
            auto begin = std::chrono::steady_clock::now();
            auto result = stat_workspace(entry->getWSPath(), thread_count);
            auto end = std::chrono::steady_clock::now();
            auto secs = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

            syslog(LOG_INFO, "stat for user <%s> for workspace <%s> (%ld msec, %ld files)", username.c_str(),
                   entry->getId().c_str(), secs, result.files);
            fmt::println("Id: {}", entry->getId());
            fmt::println("    workspace directory : {} ", entry->getWSPath());
            fmt::println("    files               : {}\n"
                         "    softlinks           : {}\n"
                         "    directories         : {}\n"
                         "    bytes               : {:L} ({})\n"
                         "    blocks              : {:L}",
                         result.files, result.softlinks, result.directories, result.bytes,
                         utils::prettyBytes(result.bytes), result.blocks);
            if (verbose) {
                fmt::println("\n    time[msec]          : {}", secs);
                if (secs > 0) {
                    fmt::println("    KFiles/sec          : {}", (double)result.files / secs);
                }
            }
        }
    }

    return 0;
}
