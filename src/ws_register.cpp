/*
 *  hpc-workspace-v2
 *
 *  ws_register
 *
 *  - tool to maintain links to workspaces workspaces
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
int debuglevel = 0;

int main(int argc, char** argv) {

    // options and flags
    string filesystem;
    string directory;
    string configfile;

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
        ("directory,d", po::value<string>(&directory), "target directory")
        ("configfile,c", po::value<string>(&configfile), "path to configfile");
    // clang-format on

    po::options_description secret_options("Secret");
    secret_options.add_options()("debug", "show debugging information")("trace", "show tracing information");

    // define options without names
    po::positional_options_description p;
    p.add("directory", 1);

    po::options_description all_options;
    all_options.add(cmd_options).add(secret_options);

    // parse commandline
    try {
        po::store(po::command_line_parser(argc, argv).options(all_options).positional(p).run(), opts);
        po::notify(opts);
    } catch (...) {
        fmt::println(stderr, "Usage: {} [options] DIRECTORY\n", argv[0]);
        fmt::println(stderr, "{}", cmd_options);
        exit(1);
    }

    // global flags
    debugflag = opts.count("debug");
    traceflag = opts.count("trace");

    // handle options exiting here

    if (opts.count("help") || opts.count("directory") == 0) {
        fmt::println(stderr, "Usage: {} [options] DIRECTORY\n", argv[0]);
        fmt::println(stderr, "{}", cmd_options);
        exit(0);
    }

    if (opts.count("version")) {
        utils::printVersion("ws_regiter");
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

    // main logic from here

    if (!cppfs::exists(cppfs::path(directory))) {
        cppfs::create_directories(cppfs::path(directory));
    }

    // get user and groups
    string username = user::getUsername(); // used for rights checks
    auto grouplist = user::getGrouplist();

    for (auto const& fs : config.validFilesystems(username, grouplist, ws::LIST)) {
        std::vector<string> keeplist, createlist;
        if (!cppfs::exists(cppfs::path(directory) / fs)) {
            cppfs::create_directories(cppfs::path(directory) / fs);
        }

        // create links
        std::unique_ptr<Database> db;
        try {
            db = std::unique_ptr<Database>(config.openDB(fs));
        } catch (DatabaseException& e) {
            spdlog::error(e.what());
            continue;
        }
        for (auto const& id : db->matchPattern("*", username, grouplist, false, false)) {
            try {
                std::unique_ptr<DBEntry> entry(db->readEntry(id, false));
                if (entry) {
                    auto wsname = entry->getWSPath();
                    auto linkpath = cppfs::path(directory) / fs / cppfs::path(wsname).filename();
                    keeplist.push_back(linkpath);
                    if (!cppfs::exists(linkpath) && !cppfs::is_symlink(linkpath)) {
                        fmt::println("creating link {}", linkpath.string());
                        cppfs::create_symlink(wsname, linkpath);
                        createlist.push_back(linkpath);
                    }
                }
            } catch (DatabaseException& e) {
                spdlog::error(e.what());
            }
        }

        // delete links not beeing workspaces anymore
        for (auto const& f : utils::dirEntries(cppfs::path(directory) / fs, username + "-*", false)) {
            auto fullpath = cppfs::path(directory) / fs / f;
            if (cppfs::is_symlink(fullpath)) {
                if (canFind(keeplist, fullpath)) {
                    if (!canFind(createlist, fullpath)) {
                        fmt::println("keeping link {}", fullpath.string());
                    }
                } else {
                    fmt::println("removing link {}", fullpath.string());
                    cppfs::remove(fullpath);
                }
            }
        }
    }
}
