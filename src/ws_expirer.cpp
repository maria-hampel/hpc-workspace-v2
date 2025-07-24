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

#include <algorithm>
#include <filesystem>
#include <string>
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

struct clean_stray_result_t {
    long valid_ws;
    long invalid_ws;
    long valid_deleted;
    long invalid_deleted;
};

// clean_stray_directtories
//  finds directories that are not in DB and removes them,
//  returns numbers of valid and invalid directories
static clean_stray_result_t clean_stray_directories(Config config, const std::string fs, const bool dryrun) {

    clean_stray_result_t result = {0, 0, 0, 0};

    // helper to store space and found dir together
    struct dir_t {
        std::string space;
        std::string dir;
    };

    std::vector<string> spaces = config.getFsConfig(fs).spaces;
    std::vector<dir_t> dirs; // list of all directories in all spaces of 'fs'

    //////// stray directories /////////
    // move directories not having a DB entry to deleted

    fmt::println("Stray directory removal for filesystem {}", fs);
    fmt::println("  workspaces first...");

    // find directories first, check DB entries later, to prevent data race with workspaces
    // getting created while this is running
    for (const auto& space : spaces) {
        // NOTE: *-* for compatibility with old expirer
        for (const auto& dir : utils::dirEntries(space, "*-*")) {
            if (cppfs::is_directory(dir)) {
                dirs.push_back({space, dir});
            }
        }
    }

    // check for magic in DB entry, to avoid that DB is not existing and all workspaces
    // get wiped by accident, e.g. due to mouting problems if DB is not in same FS as workspaces
    if (!cppfs::exists(cppfs::path(config.getFsConfig(fs).database) / ".ws_db_magic")) {
        auto magic = utils::getFirstLine(utils::getFileContents(config.getFsConfig(fs).database + "/.ws_db_magic"));
        if (magic != fs) {
            spdlog::error("DB directory {} from fs {} does not contain .ws_db_magic with correct workspace name in it, "
                          "skipping to avoid data loss!",
                          config.getFsConfig(fs).database, fs);
            // FIXME: TODO: senderrormail( /* same error */);
        }
    } else {
        spdlog::error("DB directory {} from fs {} does not contain .ws_db_magic, skipping to avoid data loss!",
                      config.getFsConfig(fs).database, fs);
        return result; // early exit, do nothing here
    }

    // get all workspace pathes from DB
    std::unique_ptr<Database> db(config.openDB(fs));
    auto wsIDs = db->matchPattern("*", "*", {}, false, false); // (1)
    std::vector<std::string> workspacesInDB;

    workspacesInDB.reserve(wsIDs.size());
    for (auto const& wsid : wsIDs) {
        // FIXME: this can throw in cases of bad config
        workspacesInDB.push_back(db->readEntry(wsid, false)->getWSPath());
    }

    // compare filesystem with DB
    for (auto const& founddir : dirs) { // (2)
        // TODO: check if in right space
        if (!canFind(workspacesInDB, founddir.dir)) {
            fmt::println("    stray workspace ", founddir.dir);

            // FIXME: a stray workspace will be moved to deleted here, and will be deleted in
            // the same run in (3). Is this intended? dangerous with datarace #87

            string timestamp = fmt::format("{}", time(NULL));
            if (!dryrun) {
                try {
                    fmt::println("      move {} to {}", founddir.dir,
                                 (cppfs::path(founddir.space) / config.deletedPath(fs) / timestamp).string());
                    cppfs::rename(founddir.dir, cppfs::path(founddir.space) / config.deletedPath(fs));
                } catch (cppfs::filesystem_error& e) {
                    spdlog::error("      failed to move to deleted: {} ({})", founddir.dir, e.what());
                }
            } else {
                fmt::println("      would move {} to {}", founddir.dir,
                             (cppfs::path(founddir.space) / config.deletedPath(fs) / timestamp).string());
            }
            result.invalid_ws++;
        } else {
            result.valid_ws++;
        }
    }

    fmt::println("    {} valid, {} invalid directories found.", result.valid_ws, result.invalid_ws);

    fmt::println("  ... deleted workspaces second...");

    ///// deleted workspaces /////  (3)
    // delete deleted workspaces that no longer have any DB entry
    // ws_release moves DB entry first, workspace second, should be race free
    dirs.clear();
    // directory entries first
    for (auto const& space : spaces) {
        // TODO: check correct space here
        // NOTE: *-* for compatibility with old expirer
        for (const auto& dir : utils::dirEntries(cppfs::path(space) / config.deletedPath(fs), "*-*")) {
            if (cppfs::is_directory(dir)) {
                dirs.push_back({space, dir});
            }
        }
    }

    // get all workspace names from DB, this contains the timestamp
    wsIDs = db->matchPattern("*", "*", {}, true, false);
    workspacesInDB.clear();
    for (auto const& wsid : wsIDs) {
        workspacesInDB.push_back(wsid);
    }

    // compare filesystem with DB
    for (auto const& founddir : dirs) {
        // TODO: check for right space
        if (!canFind(workspacesInDB, cppfs::path(founddir.dir).filename())) {
            fmt::println("    stray workspace ", founddir.dir);
            if (!dryrun) {
                try {
                    // FIXME: is that safe against symlink attacks?
                    // TODO: add timeout
                    utils::rmtree(cppfs::path(founddir.space) / config.deletedPath(fs));
                    fmt::println("      remove ", founddir.dir);
                } catch (cppfs::filesystem_error& e) {
                    spdlog::error("      failed to remove: {} ({})", founddir.dir, e.what());
                }
            } else {
                fmt::println("      would remove ", founddir.dir);
            }
            result.invalid_deleted++;
        } else {
            result.valid_deleted++;
        }
    }

    fmt::println("    {} valid, {} invalid directories found.", result.valid_deleted, result.invalid_deleted);

    return result;
}

// expire workspace DB entries and moves the workspace to deleted directory
// deletes expired workspace in second phase
static expire_result_t expire_workspaces(Config config, const string fs, const bool dryrun) {

    expire_result_t result = {0, 0, 0};

    vector<string> spaces = config.getFsConfig(fs).spaces;

    std::unique_ptr<Database> db(config.openDB(fs));

    fmt::println("Checking DB for workspaces to be expired for filesystem {}", fs);

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
    std::vector<string> filesystem;
    std::string configfile;
    bool dryrun = true;

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
        ("filesystems,F", po::value<vector<string>>(&filesystem), "filesystems/workspaces to delete from")
        ("cleaner,c", "no dry-run mode")
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

    // global flags
    debugflag = opts.count("debug");
    traceflag = opts.count("trace");

    // handle options exiting here

    if (opts.count("help")) {
        fmt::println(stderr, "Usage: {} [options]\n", argv[0]);
        fmt::println(stderr, "{}", cmd_options);
        exit(0);
    }

    if (opts.count("version")) {
        utils::printVersion("ws_expirer");
        utils::printBuildFlags();
        exit(0);
    }

    if (opts.count("cleaner")) {
        cleanermode = true;
        dryrun = false;
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

    // main logic from here
    std::vector<std::string> fslist;

    if (opts.count("filesystem")) {
        // use only valid filesystems from commmand line
        for (auto const& fs : filesystem) {
            if (canFind(config.Filesystems(), fs))
                fslist.push_back(fs);
        }
    } else {
        fslist = config.Filesystems();
    }

    if (debugflag)
        spdlog::debug("fslist: {}", fslist);

    if (cleanermode) {
        fmt::println("really cleaning!");
    } else {
        fmt::println("simulate cleaning - dryrun");
    }

    // go through filesystem and
    // - delete stray directories first (directories with no DB entry)
    // - delete deleted ones not in DB
    // this searches over filesystem and checks DB
    for (auto const& fs : fslist) {
        clean_stray_directories(config, fs, dryrun);
    }

    // go through database and
    // - expire workspaces beyond expiration age and
    // - delete expired ones which are beyond keep date
    for (auto const& fs : fslist) {
        expire_workspaces(config, fs, dryrun);
    }

    return 0;
}
