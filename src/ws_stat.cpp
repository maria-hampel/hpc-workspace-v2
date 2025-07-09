/*
 *  hpc-workspace-v2
 *
 *  ws_stat
 *
 *  - tool to get statistics about workspaces
 *
 *  this tool contains hardcoded lustre ABI information, as lustre headers do not compile properly with C++
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *
 *  (c) Holger Berger 2021,2023,2024,2025
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

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>

#include "config.h"
#include <boost/program_options.hpp>

#include "build_info.h"
#include "db.h"
#include "fmt/format.h"  // IWYU pragma: keep
#include "fmt/ostream.h" // IWYU pragma: keep
#include "fmt/ranges.h"  // IWYU pragma: keep

#include "user.h"

#include "caps.h"
#include "ws.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <sys/xattr.h>
#include <unistd.h>

// init caps here, when euid!=uid
Cap caps{};

namespace po = boost::program_options;
namespace cppfs = std::filesystem;

using namespace std;

bool debugflag = false;
bool traceflag = false;
bool verbose = false;

// return type for stat collection
struct stat_result {
    uint64_t files;
    uint64_t softlinks;
    uint64_t directories;
    uint64_t bytes;
};

// return filesize using statx(), this works only from redhat >= 8
uint64_t getfilesize(const char* path) {
    struct statx statxbuf;
    // static bool analyzed = false;

    int mask = STATX_SIZE;
    int flags = AT_STATX_DONT_SYNC;

    int ret = statx(0, path, flags, mask, &statxbuf);
    /*
        if (!analyzed) {
            analyzed = true;
            fmt::println(stderr, "statx({}) -> {:b}", path, mask);
        }
    */
    if (ret == 0) {
        return statxbuf.stx_size;
    } else {
        return 0;
    }
}

// this would work for rh7, no logic implemented to call it
uint64_t getfilesize_compat(const char* path) {
    struct stat statbuf;
    int ret = stat(path, &statbuf);
    if (ret == 0) {
        return statbuf.st_size;
    } else {
        return 0;
    }
}

// do recursive BFS directory traversal and statx
// count bytes, files, symlinks, directories
void parallel_stat(struct stat_result& result, string path) {
    std::vector<cppfs::path> dirs;
    dirs.reserve(128);
    std::vector<cppfs::path> files;
    files.reserve(1024);

    // thread local variables
    uint64_t bytes = 0;
    uint64_t nrfiles = 0;
    uint64_t softlinks = 0;
    uint64_t directories = 0;

    if (cppfs::is_directory(path)) {
        for (const auto& entry : cppfs::directory_iterator(path)) {
            if (entry.is_regular_file()) {
                nrfiles++;
                files.push_back(entry);
            } else if (entry.is_symlink()) {
                softlinks++;
            } else if (entry.is_directory()) {
                directories++;
                dirs.push_back(entry);
            }
        }
    } else {
        fmt::println(stderr, "Error    : workspace <{}> does not exist!", path);
    }

    // do the statx here, parallel
#pragma omp parallel reduction(+ : bytes)
    for (const auto& entry : files) {
        bytes += getfilesize(entry.c_str());
    }

    // update shared result
#pragma omp critical
    {
        result.bytes += bytes;
        result.files += nrfiles;
        result.softlinks += softlinks;
        result.directories += directories;
    }

    // recurse into directories, parallel
    for (const auto& dir : dirs) {
#pragma omp task shared(result)
        parallel_stat(result, dir);
    }
}

// dive into parallel decent
struct stat_result stat_workspace(string wspath) {
    struct stat_result result = {0L, 0L, 0L, 0L};

#pragma omp parallel shared(result)
    {
#pragma omp single
        parallel_stat(result, wspath);
    }
    return result;
}

// helper for fmt::
template <> struct fmt::formatter<po::options_description> : ostream_formatter {};

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

    // define options
    po::options_description cmd_options("\nOptions");
    // clang-format off
    cmd_options.add_options()
        ("help,h", "produce help message")
        ("version,V", "show version")
        ("filesystem,F", po::value<string>(&filesystem), "filesystem to list workspaces from")
        ("user,u", po::value<string>(&user), "only show workspaces for selected user")
        ("group,g", "enable listing of group workspaces")
        ("config", po::value<string>(&configfile), "config file")
        ("pattern,p", po::value<string>(&pattern), "pattern matching name (glob syntax)")
        ("name,N", "sort by name")
        ("creation,C", "sort by creation date")
        ("remaining,R", "sort by remaining time")
        ("reverted,r", "revert sort")
        ("verbose,v", "verbose listing");
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

#ifndef WS_ALLOW_USER_DEBUG // FIXME: implement this in CMake
    if (user::isRoot()) {
#else
    {
#endif
        debugflag = opts.count("debug");
        traceflag = opts.count("trace");
    }

    // handle options exiting here

    if (opts.count("help")) {
        fmt::print(stderr, "Usage: {} [options] [pattern]\n", argv[0]);
        fmt::println(stderr, "{}", cmd_options);
        exit(0);
    }

    if (opts.count("version")) {
#ifdef IS_GIT_REPOSITORY
        fmt::println("workspace build from git commit hash {} on top of release {}", GIT_COMMIT_HASH, WS_VERSION);
#else
        fmt::println("workspace version {}", WS_VERSION);
#endif
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
            fmt::print(stderr, "Warning: ignored config file options!\n");
        }
    }

    auto config = Config(configfilestoread);
    if (!config.isValid()) {
        fmt::println(stderr, "Error  : No valid config file found!");
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
            fmt::println(stderr, "Error  : invalid filesystem given.");
        }
    } else {
        fslist = validfs;
    }

    vector<std::unique_ptr<DBEntry>> entrylist;

    // iterate over filesystems
    for (auto const& fs : fslist) {
        if (debugflag)
            fmt::print("Debug  : loop over fslist {} in {}\n", fs, fslist);
        std::unique_ptr<Database> db(config.openDB(fs));

#pragma omp parallel for schedule(dynamic)
        for (auto const& id : db->matchPattern(pattern, userpattern, grouplist, false, listgroups)) {
            try {
                std::unique_ptr<DBEntry> entry(db->readEntry(id, false));
                // if entry is valid
                if (entry) {
#pragma omp critical
                    {
                        entrylist.push_back(std::move(entry));
                    }
                }
            } catch (DatabaseException& e) {
                fmt::println(e.what());
            }
        }

    } // loop over fs

    if (sortbyremaining)
        std::sort(entrylist.begin(), entrylist.end(),
                  [](const auto& x, const auto& y) { return (x->getRemaining() < y->getRemaining()); });
    if (sortbycreation)
        std::sort(entrylist.begin(), entrylist.end(),
                  [](const auto& x, const auto& y) { return (x->getCreation() < y->getCreation()); });
    if (sortbyname)
        std::sort(entrylist.begin(), entrylist.end(),
                  [](const auto& x, const auto& y) { return (x->getId() < y->getId()); });

    if (sortreverted) {
        std::reverse(entrylist.begin(), entrylist.end());
    }

    bool sort = sortbyname || sortbycreation || sortbyremaining;

    std::locale::global(std::locale("en_US.UTF-8"));

    if (sort) {
        for (auto const& entry : entrylist) {
            fmt::println("Id: {}", entry->getId());
            fmt::println("    workspace directory : {} ", entry->getWSPath());
            auto begin = std::chrono::steady_clock::now();
            auto result = stat_workspace(entry->getWSPath());
            auto end = std::chrono::steady_clock::now();
            auto secs = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
            fmt::println("    files               : {}\n"
                         "    softlinks           : {}\n"
                         "    directories         : {}\n"
                         "    bytes               : {:L}",
                         result.files, result.softlinks, result.directories, result.bytes);
            if (verbose) {
                fmt::println("    time[msec]          : {}", secs);
                fmt::println("    KFiles/sec          : {}", (double)result.files / secs);
            }
        }
    } else {
#pragma omp parallel for schedule(dynamic)
        for (auto const& entry : entrylist) {
            auto begin = std::chrono::steady_clock::now();
            auto result = stat_workspace(entry->getWSPath());
            auto end = std::chrono::steady_clock::now();
            auto secs = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
#pragma omp critical
            {
                fmt::println("Id: {}", entry->getId());
                fmt::println("    workspace directory : {} ", entry->getWSPath());
                fmt::println("    files               : {}\n"
                             "    softlinks           : {}\n"
                             "    directories         : {}\n"
                             "    bytes               : {:L}",
                             result.files, result.softlinks, result.directories, result.bytes);
                if (verbose) {
                    fmt::println("    time[msec]          : {}", secs);
                    fmt::println("    KFiles/sec          : {}", (double)result.files / secs);
                }
            }
        }
    }
}
