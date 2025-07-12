/*
 *  hpc-workspace-v2
 *
 *  ws_expirer
 *
 *  - tool to expire and delete workspaces from a cronjob
 *    changes to workspace++:
 *      - c++ implementation (not python anymore)
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

#include <algorithm>
#include <filesystem>
#include <vector>

#include "config.h"
#include <boost/program_options.hpp>

#include "build_info.h"
#include "db.h"
#include "fmt/base.h"
#include "fmt/ostream.h"
#include "fmt/ranges.h" // IWYU pragma: keep
#include "user.h"

#include "caps.h"
#include "utils.h"
#include "ws.h"

#include "spdlog/spdlog.h"

// init caps here, when euid!=uid
Cap caps{};

namespace po = boost::program_options;
namespace cppfs = std::filesystem;

// helper for fmt::
template <> struct fmt::formatter<po::options_description> : ostream_formatter {};

using namespace std;

bool debugflag = false;
bool traceflag = false;

bool cleanermode = false;

struct expire_result_t {
    long expired_ws;
    long kept_ws;
    long deleted_ws;
};

// expire workspace DB entries and moves the workspace to deleted directory
// deletes expired workspace in second phase
static expire_result_t expire_workspaces(Config config, const string fs, const bool dryrun) {

    expire_result_t result = {0, 0, 0};

    vector<string> spaces = config.getFsConfig(fs).spaces;

    auto db = config.openDB(fs);

    fmt::println("Checking DB for workspaces to be expired for ", fs);

    // search expired active workspaces in DB
    for (auto const& id : db->matchPattern("*", "*", {}, false, false)) {
        std::unique_ptr<DBEntry> dbentry;
        // error logic first, we skip all loop body in case of bad entry
        try {
            dbentry = std::unique_ptr<DBEntry>(db->readEntry(id, false));
            if (!dbentry) {
                spdlog::error("skipping db entry {}", id);
                continue;
            }
        } catch (DatabaseException& e) {
            spdlog::error(e.what());
            spdlog::error("skipping db entry {}", id);
            continue;
        }

        // entry is good

        auto expiration = dbentry->getExpiration();

        if (expiration <= 0) {
            spdlog::error("bad expiration in {}, skipping", id);
            continue;
        }

        // do we have to expire?
        if (time((long*)0L) > expiration) {
            auto timestamp = to_string(time((long*)0L));
            fmt::println(" expiring {} (expired {})", id, ctime(&expiration));
            result.expired_ws++;
            if (!dryrun) {
                // db entry first
                dbentry->expire(timestamp);

                // workspace second
                auto wspath = dbentry->getWSPath();
                try {
                    auto tgt = cppfs::path(wspath) / config.deletedPath(fs) /
                               (cppfs::path(wspath).filename().string() + "-" + timestamp);
                    if (debugflag) {
                        spdlog::debug("mv ", wspath, " -> ", tgt.string());
                    }
                    cppfs::rename(wspath, tgt);
                } catch (cppfs::filesystem_error& e) {
                    spdlog::error("failed to move workspace: {} ({})", wspath, e.what());
                }
            }
        } else {
            fmt::println(" keeping {}", id); // TODO: add expiration time
            result.kept_ws++;
            // TODO: reminder mails
        }
    }

    fmt::println("  {} workspaces expired, {} kept.", result.expired_ws, result.kept_ws);

    fmt::println("Checking deleted DB for workspaces to be deleted for {}", fs);

    // search in DB for expired/released workspaces for those over keeptime to delete them
    for (auto const& id : db->matchPattern("*", "*", {}, true, false)) {
        std::unique_ptr<DBEntry> dbentry;
        try {
            dbentry = std::unique_ptr<DBEntry>(db->readEntry(id, true));
            if (!dbentry) {
                spdlog::error("skipping db entry {}", id);
                continue;
            }
        } catch (DatabaseException& e) {
            spdlog::error(e.what());
            spdlog::error("skipping db entry {}", id);
            continue;
        }

        auto expiration = dbentry->getExpiration();
        auto releasetime = dbentry->getReleaseTime();
        auto keeptime = config.getFsConfig(fs).keeptime;

        // get released time from name = id
        try {
            releasetime = std::stol(utils::splitString(id, '-').at(
                std::count(id.begin(), id.end(), '-'))); // count from back, for usernames with "-"
        } catch (const out_of_range& e) {
            spdlog::error("skipping DB entry with unparsable name {}", id);
            continue;
        }

        auto released = dbentry->getReleaseTime(); // check if it was released by user
        if (released > 1000000000L) {              // released after 2001? if not ignore it
            releasetime = released;
        } else {
            releasetime = 3000000000L; // date in future, 2065
            fmt::println(stderr, "  IGNORING released {} for {}", releasetime, id);
        }

        if ((time((long*)0L) > (expiration + keeptime * 24 * 3600)) || (time((long*)0L) > releasetime + 3600)) {

            result.deleted_ws++;

            if (time((long*)0L) > releasetime + 3600) {
                fmt::println(" deleting DB entry {}, was released ", id, ctime(&releasetime));
            } else {
                fmt::println(" deleting DB entry {}, expired ", id, ctime(&expiration));
            }
            if (cleanermode) {
                db->deleteEntry(id, true);
            }

            auto wspath = cppfs::path(dbentry->getWSPath()) / config.getFsConfig(fs).deletedPath / id;
            fmt::println(" deleting directory: {}", wspath.string());
            if (cleanermode) {
                try {
                    utils::rmtree(wspath.string());
                } catch (cppfs::filesystem_error& e) {
                    fmt::println(stderr, "  failed to remove: {} ({})", wspath.string(), e.what());
                }
            }
        } else {
            fmt::println(" (keeping restorable {})", id); // TODO: add expiration + keeptime
        }
    }

    fmt::println("  {} workspaces deleted.", result.deleted_ws);

    return result;
}

int main(int argc, char** argv) {

    // options and flags
    string filesystem;
    string configfile;

    po::variables_map opts;

    // locals settings to prevent strange effects
    utils::setCLocal();

    // set custom logging format
    utils::setupLogging();

    // define options
    po::options_description cmd_options("\nOptions");
    // clang-format off
     cmd_options.add_options()
        ("help,h", "produce help message")
        ("version,V", "show version")
        ("workspaces,w", po::value<string>(&filesystem), "filesystem/workspace to delete from")
        ("cleaner,c", "target directory")
        ("configfile", po::value<string>(&configfile), "path to configfile");
    // clang-format on

    po::options_description secret_options("Secret");
    secret_options.add_options()("debug", "show debugging information")("trace", "show tracing information");

    po::options_description all_options;
    all_options.add(cmd_options).add(secret_options);

    // parse commandline
    try {
        po::store(po::command_line_parser(argc, argv).options(all_options).run(), opts);
        po::notify(opts);
    } catch (...) {
        fmt::println(stderr, "Usage: {} [options]\n", argv[0]);
        fmt::println(stderr, "{}", cmd_options);
        exit(1);
    }

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
        fmt::println(stderr, "Usage: {} [options]\n", argv[0]);
        fmt::println(stderr, "{}", cmd_options);
        exit(0);
    }

    if (opts.count("cleaner"))
        cleanermode = true;

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

    // main logic from here
}
