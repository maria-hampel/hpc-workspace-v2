/*
 *  hpc-workspace-v2
 *
 *  ws_stat
 *
 *  - tool to get statistics about workspaces (bshoshany/thread-pool version)
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
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <utility>
#include <vector>

// Include bshoshany thread-pool
#define BS_THREAD_POOL_NATIVE_EXTENSIONS
#include "BS_thread_pool.hpp"

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

// Helper function to collect all entries recursively (for serial subtree processing)
void collect_all_entries_recursive(const cppfs::path& path, std::vector<cppfs::path>& files, uint64_t& softlinks,
                                   uint64_t& directories) {
    if (!cppfs::is_directory(path)) {
        return;
    }

    for (const auto& entry : cppfs::directory_iterator(path)) {
        if (entry.is_symlink()) {
            softlinks++;
        } else if (entry.is_directory()) {
            directories++;
            collect_all_entries_recursive(entry.path(), files, softlinks, directories);
        } else if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
}

// Collect only files and subdirectories at current level (no recursion)
void collect_level(const cppfs::path& path, std::vector<cppfs::path>& files, std::vector<cppfs::path>& subdirs,
                   uint64_t& softlinks, uint64_t& directories) {
    if (!cppfs::is_directory(path)) {
        return;
    }

    for (const auto& entry : cppfs::directory_iterator(path)) {
        if (entry.is_symlink()) {
            softlinks++;
        } else if (entry.is_directory()) {
            directories++;
            subdirs.push_back(entry.path());
        } else if (entry.is_regular_file()) {
            files.push_back(entry.path());
        }
    }
}

// Process entire subtree serially (for depth-limited parallel traversal)
StatResult process_subtree_serial(const cppfs::path& path) {
    std::vector<cppfs::path> all_files;
    uint64_t softlinks = 0;
    uint64_t directories = 0;

    collect_all_entries_recursive(path, all_files, softlinks, directories);

    StatResult result;
    result.directories = directories;
    result.softlinks = softlinks;

    for (const auto& entry : all_files) {
        auto [bytes, blocks] = getfilesize(entry);
        result.files++;
        result.bytes += bytes;
        result.blocks += blocks;
    }

    return result;
}

// Depth-limited parallel directory traversal
StatResult process_directory_parallel(const cppfs::path& path, unsigned int current_depth, unsigned int max_depth,
                                      BS::thread_pool<BS::tp::none>& dir_pool) {
    std::vector<cppfs::path> files, subdirs;
    uint64_t softlinks = 0;
    uint64_t directories = 0;

    // At max depth: process entire subtree serially (includes all counting)
    if (current_depth >= max_depth) {
        return process_subtree_serial(path);
    }

    // Below max depth: collect this level and recurse
    collect_level(path, files, subdirs, softlinks, directories);

    // Process files at this level
    StatResult result;
    result.directories = directories;
    result.softlinks = softlinks;

    for (const auto& entry : files) {
        auto [bytes, blocks] = getfilesize(entry);
        result.files++;
        result.bytes += bytes;
        result.blocks += blocks;
    }

    // Submit subdirectories in parallel
    std::vector<std::future<StatResult>> futures;
    for (const auto& subdir : subdirs) {
        try {
            auto fut = dir_pool.submit_task([subdir, current_depth, max_depth, &dir_pool]() {
                return process_directory_parallel(subdir, current_depth + 1, max_depth, dir_pool);
            });
            futures.push_back(std::move(fut));
        } catch (const std::exception& e) {
            spdlog::warn("Failed to enqueue subdirectory {}: {} - data might be incomplete", subdir.string(), e.what());
        }
    }

    // Wait and aggregate results
    for (auto& fut : futures) {
        try {
            result += fut.get();
        } catch (const std::exception& e) {
            spdlog::warn("Subdirectory processing failed: {} - data might be incomplete", e.what());
        }
    }

    return result;
}

// Wrapper function
StatResult stat_workspace(const std::string& wspath, BS::thread_pool<BS::tp::none>& dir_pool, unsigned int max_depth) {
    if (!cppfs::is_directory(wspath)) {
        spdlog::error("workspace <{}> does not exist!", wspath);
        return StatResult{};
    }

    return process_directory_parallel(wspath, 0, max_depth, dir_pool);
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
    int max_depth_param = -1; // -1 means auto-calculate

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
        ("max-depth,m", po::value<int>(&max_depth_param)->default_value(-1), "max parallel traversal depth (-1 = auto)")
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

    // Calculate max_depth from thread count if not specified
    unsigned int max_depth;
    if (max_depth_param < 0) {
        max_depth = std::min(6u, std::max(3u, thread_count / 4));
        if (debugflag) {
            spdlog::debug("Derived max_depth={} from threads={}", max_depth, thread_count);
        }
    } else {
        max_depth = static_cast<unsigned int>(max_depth_param);
        if (max_depth < 3) {
            spdlog::warn("max_depth {} is below minimum of 3, using 3", max_depth_param);
            max_depth = 3;
        }
        if (debugflag) {
            spdlog::debug("Using user-specified max_depth={}", max_depth);
        }
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

    // Create two thread pools: workspace_pool for workspace-level parallelism,
    // dir_pool for directory-level parallelism (shared across workspaces)
    BS::thread_pool<BS::tp::none> workspace_pool(thread_count);
    BS::thread_pool<BS::tp::none> dir_pool(thread_count);
    if (debugflag) {
        spdlog::debug("Creating workspace_pool and dir_pool with {} threads each", thread_count);
    }

    // Process workspaces
    if (!sort && entrylist.size() > 1) {
        if (debugflag) {
            spdlog::debug("Parallel workspace processing with {} workspaces", entrylist.size());
        }
        workspace_pool
            .submit_loop(0, entrylist.size(),
                         [&entrylist, &username, &dir_pool, max_depth](size_t i) {
                             auto begin = std::chrono::steady_clock::now();
                             auto result = stat_workspace(entrylist[i]->getWSPath(), dir_pool, max_depth);
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
            auto result = stat_workspace(entry->getWSPath(), dir_pool, max_depth);
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
