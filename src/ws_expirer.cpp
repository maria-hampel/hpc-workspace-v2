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
#include <memory>
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

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/syslog_sink.h" // IWYU pragma: keep
#include "spdlog/sinks/daily_file_sink.h" // IWYU pragma: keep
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
int debuglevel = 0;

bool cleanermode = false;

struct expire_result_t {
    long expired_ws;
    long kept_ws;
    long deleted_ws;
    long sent_mails;
    long bad_db;

    expire_result_t& operator+=(const expire_result_t& other) {
        expired_ws += other.expired_ws;
        kept_ws += other.kept_ws;
        deleted_ws += other.deleted_ws;
        sent_mails += other.sent_mails;
        bad_db += other.bad_db;
        return *this; // Return a reference to the modified object
    }
};

struct clean_stray_result_t {
    long valid_ws;
    long invalid_ws;
    long valid_deleted;
    long invalid_deleted;

    clean_stray_result_t& operator+=(const clean_stray_result_t& other) {
        valid_ws += other.valid_ws;
        invalid_ws += other.invalid_ws;
        valid_deleted += other.valid_deleted;
        invalid_deleted += other.invalid_deleted;
        return *this; // Return a reference to the modified object
    }
};

// time to keep released workspaces before deletion in seconds
long releasekeeptime = 3600;


// own logging setup,
// logs in color to console
// and into a daily rotating file with timestamps
static void setupLogging() {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("%^%l%$: %v");
    auto file_sink = std::make_shared<spdlog::sinks::daily_file_format_sink_mt>("/var/log/ws_expirer.log", 0, 1); // FIXME: TODO: make path a config paramer or command line argument
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    spdlog::logger* log = new spdlog::logger("ws_expirer", {file_sink, console_sink});
    spdlog::set_default_logger(std::shared_ptr<spdlog::logger>(log));
    spdlog::set_level(spdlog::level::trace);
}

// clean_stray_directtories
//  finds directories that are not in DB and removes them,
//  returns numbers of valid and invalid directories
//  this searches over filesystem and compares with DB, checks if a valid DB is available (using a magic file)
static clean_stray_result_t clean_stray_directories(const Config& config, const std::string fs,
                                                    const std::string single_space, const bool dryrun) {

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

    spdlog::info("Stray directory removal for filesystem {}", fs);
    spdlog::info("  workspaces first...");

    if (single_space != "") {
        if (canFind(spaces, single_space)) {
            spdlog::info("only cleaning in space {}", single_space);
            spaces = {single_space};
        } else {
            spdlog::info("given space not in filesystem {}, skipping.", fs);
            return {0, 0, 0, 0};
        }
    }

    // find directories first, check DB entries later, to prevent data race with
    // workspaces getting created while this is running
    for (const auto& space : spaces) {
        // NOTE: *-* for compatibility with old expirer
        for (const auto& dir : utils::dirEntries(space, "*-*")) {
            if (cppfs::is_directory(dir)) {
                dirs.push_back({space, dir});
            }
        }
    }

    std::unique_ptr<Database> db;
    // check for errors, if this throws DB is invalid and we should skip this DB
    try {
        db = std::unique_ptr<Database>(config.openDB(fs));
    } catch (DatabaseException& e) {
        spdlog::error(e.what());
        spdlog::error("skipping, to avoid data loss");
        // TODO: senderrormail ...
        return result;
    }

    // get all workspace pathes from DB
    auto wsIDs = db->matchPattern("*", "*", {}, false, false); // (1)
    std::vector<std::string> workspacesInDB;

    workspacesInDB.reserve(wsIDs.size());
    for (auto const& wsid : wsIDs) {
        // FIXME: this can throw in cases of bad config
        workspacesInDB.push_back(db->readEntry(wsid, false)->getWSPath());
    }

    // compare filesystem with DB
    for (auto const& founddir : dirs) { // (2)
        if (!canFind(workspacesInDB, founddir.dir)) {
            spdlog::warn("    stray workspace ", founddir.dir);

            // FIXME: a stray workspace will be moved to deleted here, and will be deleted in
            // the same run in (3). Is this intended? dangerous with datarace #87

            string timestamp = fmt::format("{}", time(NULL));
            if (!dryrun) {
                try {
                    spdlog::info(
                        "      move {} to {}", founddir.dir,
                        (cppfs::path(founddir.space).remove_filename() / config.deletedPath(fs) / timestamp).string());
                    cppfs::rename(founddir.dir, cppfs::path(founddir.space).remove_filename() / config.deletedPath(fs));
                } catch (cppfs::filesystem_error& e) {
                    spdlog::error("      failed to move to deleted: {} ({})", founddir.dir, e.what());
                }
            } else {
                spdlog::info(
                    "      would move {} to {}", founddir.dir,
                    (cppfs::path(founddir.space).remove_filename() / config.deletedPath(fs) / timestamp).string());
            }
            result.invalid_ws++;
        } else {
            result.valid_ws++;
        }
    }

    spdlog::info("    {} valid, {} invalid directories found.", result.valid_ws, result.invalid_ws);

    spdlog::info("  ... deleted workspaces second...");

    ///// deleted workspaces /////  (3)
    // delete deleted workspaces that no longer have any DB entry
    // ws_release moves DB entry first, workspace second, should be race free
    dirs.clear();
    // directory entries first
    for (auto const& space : spaces) {
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
        if (!canFind(workspacesInDB, cppfs::path(founddir.dir).filename())) {
            spdlog::warn("    stray removed workspace ", founddir.dir);
            if (!dryrun) {
                try {
                    // TODO: add timeout
                    utils::rmtree(cppfs::path(founddir.space) / config.deletedPath(fs));
                    spdlog::info("      remove ", founddir.dir);
                } catch (cppfs::filesystem_error& e) {
                    spdlog::error("      failed to remove: {} ({})", founddir.dir, e.what());
                }
            } else {
                spdlog::info("      would remove ", founddir.dir);
            }
            result.invalid_deleted++;
        } else {
            result.valid_deleted++;
        }
    }

    spdlog::info("    {} valid expired, {} invalid expired directories found.", result.valid_deleted,
                 result.invalid_deleted);
    return result;
}

// expire workspace DB entries and moves the workspace to deleted directory
// deletes expired workspace in second phase
static expire_result_t expire_workspaces(const Config& config, const string fs, const bool dryrun) {

    expire_result_t result = {0, 0, 0, 0, 0};

    // vector<string> spaces = config.getFsConfig(fs).spaces;

    std::unique_ptr<Database> db;
    // check for errors, if this throws DB is invalid and we should skip this DB
    try {
        db = std::unique_ptr<Database>(config.openDB(fs));
    } catch (DatabaseException& e) {
        spdlog::error(e.what());
        spdlog::error("skipping, to avoid data loss");
        // TODO: senderrormail ...
        return result;
    }

    spdlog::info("Checking DB for workspaces to be expired for filesystem {}", fs);

    // search expired active workspaces in DB
    for (auto const& id : db->matchPattern("*", "*", {}, false, false)) {
        std::unique_ptr<DBEntry> dbentry;
        // error logic first, we skip all loop body in case of bad entry
        try {
            dbentry = std::unique_ptr<DBEntry>(db->readEntry(id, false));
            if (!dbentry) {
                spdlog::error("skipping db entry {}", id);
                result.bad_db++;
                continue;
            }
        } catch (DatabaseException& e) {
            spdlog::error(e.what());
            spdlog::error("skipping db entry {}", id);
            result.bad_db++;
            continue;
        }

        // entry is good

        auto expiration = dbentry->getExpiration();

        if (expiration <= 0) {
            spdlog::error("bad expiration in {}, skipping", id);
            result.bad_db++;
            continue;
        }

        // do we have to expire?
        if (time((long*)0L) > expiration) {
            auto timestamp = to_string(time((long*)0L));
            spdlog::info(" expiring {} (expired {})", id, utils::trimright(ctime(&expiration)));
            result.expired_ws++;
            if (!dryrun) {
                // db entry first
                dbentry->expire(timestamp);

                // workspace second
                auto wspath = dbentry->getWSPath();
                try {
                    auto tgt = cppfs::path(wspath).remove_filename() / config.deletedPath(fs) /
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
            spdlog::info("  keeping {}     (until {})", id, utils::ctime(expiration)); // TODO: add expiration time
            result.kept_ws++;
            // TODO: reminder mails
        }
    }

    spdlog::info("=>  {} workspaces expired, {} kept.", result.expired_ws, result.kept_ws);
    spdlog::info("");
    spdlog::info("Checking deleted DB for workspaces to be deleted for filesystem {}", fs);

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

        long releasetime;
        auto expiration = dbentry->getExpiration();
        auto keeptime = config.getFsConfig(fs).keeptime;

        // get released time from name = id
        try {
            releasetime = std::stol(utils::splitString(id, '-').at(
                std::count(id.begin(), id.end(), '-'))); // count from back, for usernames with "-"
        } catch (const out_of_range& e) {
            spdlog::error("skipping DB entry with unparsable name {}", id);
            continue;
        }

        auto released = dbentry->getReleaseTime(); // check if it was released by user, 0 if not
        if (debugflag) {
            spdlog::debug("released = {}, releasetime (filename) = {}", released, releasetime);
        }
        if (released > 1000000000L) { // released after 2001? if not ignore it
            releasetime = expiration = released;
        } else if (released != 0) {    // not released at all, expired, releasetime is taken from filename
            releasetime = 3000000000L; // date in future, 2065
            spdlog::warn("  IGNORING released {} for {}", releasetime, id);
        }

        if ((time((long*)0L) > (expiration + keeptime * 24 * 3600)) ||
            (time((long*)0L) >= releasetime + releasekeeptime)) {

            result.deleted_ws++;

            if (time((long*)0L) >= releasetime + releasekeeptime) { // even a released workspace will be not deleted
                                                                    // before releasekeeptime seconds old hour old
                spdlog::info(" deleting DB entry {}, was released {}", id, utils::trimright(ctime(&releasetime)));
            } else {
                spdlog::info(" deleting DB entry {}, expired {}", id, utils::trimright(ctime(&expiration)));
            }

            if (cleanermode) {
                db->deleteEntry(id, true);
            }

            auto wspath = cppfs::path(dbentry->getWSPath()).remove_filename() / config.getFsConfig(fs).deletedPath / id;
            spdlog::info("   deleting directory: {}", wspath.string());
            if (cleanermode) {
                try {
                    utils::rmtree(wspath.string());
                } catch (cppfs::filesystem_error& e) {
                    spdlog::error("  failed to remove: {} ({})", wspath.string(), e.what());
                }
            }
        } else {
            result.kept_ws++;
            spdlog::info(
                "  keeping restorable {}     (until {})", id,
                utils::ctime(expiration + keeptime * 24 * 3600)); // TODO: is this correct for released worspaces?
        }
    }
    spdlog::info("=>  {} workspaces deleted, {} workspaces kept", result.deleted_ws, result.kept_ws);

    return result;
}

int main(int argc, char** argv) {

    // options and flags
    std::string filesystem;
    std::string single_space;
    std::string configfile;
    bool dryrun = true;

    po::variables_map opts;

    // locals settings to prevent strange effects
    utils::setCLocal();

    // set custom logging format, this different than other tools, as this tool is for root anyhow
    setupLogging();

    // define options
    po::options_description cmd_options("\nOptions");
    // clang-format off
     cmd_options.add_options()
        ("help,h", "produce help message")
        ("version,V", "show version")
        ("filesystems,F", po::value<string>(&filesystem), "filesystems/workspaces to delete from, comma separated")
        ("space,s", po::value<string>(&single_space), "path of a single space that should be deleted")
        ("cleaner,c", "no dry-run mode")
        ("configfile", po::value<string>(&configfile), "path to configfile");
    // clang-format on

    po::options_description secret_options("Secret");
    // clang-format off
    secret_options.add_options()
        ("debug", "show debugging information")
        ("trace", "show tracing information")
        ("forcedeletereleased", "option for testing");
    // clang-format on

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

    if (opts.count("forcedeletereleased")) {
        releasekeeptime = 0;
    }

    // read config
    //   user can change this if no setuid inadd expiration + keeptimestallation OR if root
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

    if (opts.count("filesystems")) {
        // use only valid filesystems from commmand line
        for (auto const& fs : utils::splitString(filesystem, ',')) {
            if (canFind(config.Filesystems(), fs))
                fslist.push_back(fs);
        }
    } else {
        fslist = config.Filesystems();
    }

    if (debugflag) {
        spdlog::debug("fslist: {}", fslist);
    }

    if (cleanermode) {
        spdlog::warn("expirer - really cleaning!");
    } else {
        spdlog::info("expirer - simulating cleaning - dryrun");
    }

    // go through filesystem and
    // - delete stray directories first (directories with no DB entry)
    // - delete deleted ones not in DB
    // this searches over filesystem and checks DB
    clean_stray_result_t total_stray = {0, 0, 0, 0};
    for (auto const& fs : fslist) {
        total_stray += clean_stray_directories(config, fs, single_space, dryrun);
    }
    spdlog::info("stray removal summary: {} valid, {} invalid, {} valid deleted, {} invalid", total_stray.valid_ws,
                 total_stray.invalid_ws, total_stray.valid_deleted, total_stray.invalid_deleted);
    spdlog::info("end of stray removal");

    // go through database and
    // - expire workspaces beyond expiration age and
    // - delete expired ones which are beyond keep date
    expire_result_t total_expire = {0, 0, 0, 0, 0};
    for (auto const& fs : fslist) {
        total_expire += expire_workspaces(config, fs, dryrun);
    }
    spdlog::info("expiratiion summary: {} kept, {} expired, {} deleted, {} reminders sent, {} bad db entries", total_expire.kept_ws,
                 total_expire.expired_ws, total_expire.deleted_ws, total_expire.sent_mails, total_expire.bad_db);
    spdlog::info("end of expiration");

    return 0;
}
