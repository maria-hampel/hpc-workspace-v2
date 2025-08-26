/*
 *  hpc-workspace-v2
 *
 *  ws_validate_config
 *
 *  -
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *
 *  (c) Holger Berger 2021,2023,2024,2025
 *  (c) Maria Hampel 2025
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


#ifdef WS_RAPIDYAML_CONFIG
    #define RYML_USE_ASSERT 0
    #include "c4/format.hpp"
    #include "c4/std/std.hpp"
    #include "ryml.hpp"
    #include "ryml_std.hpp"
#else
    #include "yaml-cpp/yaml.h"
#endif

#include "fmt/base.h"
#include "fmt/ostream.h"
#include "fmt/ranges.h" // IWYU pragma: keep
#include <iostream>
#include <string>
#include <filesystem>

#include "caps.h"
#include "utils.h"
#include "ws.h"

#include "spdlog/spdlog.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;
using namespace std;
namespace cppfs = std::filesystem;

// global variables
bool debugflag = false;
bool traceflag = false;
int debuglevel = 0;

// init caps here, when euid!=uid
Cap caps{};


int main(int argc, char** argv){
    std::string filename = "";
    // locals settings to prevent strange effects
    utils::setCLocal();

    // set custom logging format
    utils::setupLogging(string(argv[0]));

    po::variables_map opt;
    // define all options
    po::options_description cmd_options("\nOptions");
    //clang-format off
    cmd_options.add_options()
        ("help,h", "produce help message")
        ("filename,f", po::value<string>(&filename), "filename/directory");
    //clang-format on
    
    // define options without names
    po::positional_options_description p;
    p.add("filename", 1);

    // parse commandline
    try {
        po::store(po::command_line_parser(argc, argv).options(cmd_options).positional(p).run(), opt);
        po::notify(opt);
    } catch (...) {
        cerr << "Usage: " << argv[0]
             << " [filename]"
             << endl;
        cerr << cmd_options << "\n";
        exit(1);
    }

    if (opt.count("help")){
        cerr << "Usage: " << argv[0]
             << " [filename]"
             << endl;
        cerr << cmd_options << "\n";
        cerr << "this command is used to validate a config file"
             << endl;
        exit(0);
    }

    if (filename == "") {
        filename="/etc/ws.conf";
    }

    std::string yaml = utils::getFileContents(filename);
    auto config = YAML::Load(yaml);

    spdlog::info("validating config from {}", filename);

    std::vector<YAML::Node> wslist;
    std::vector<string> wsnames;

    try {
        if (config["workspaces"] || config["filesystems"]) {
            for (auto key : std::vector<string>{"workspaces", "filesystems"}){
                if(config[key]){

                    auto list = config[key];
                    wslist.push_back(list);
                    for (auto it : list){
                        wsnames.push_back(it.first.as<string>());
                    }
                }
            }
        }
    } catch (...) {
        spdlog::error("No Workspaces defined");
        exit(1);
    }

    try {
        auto clustername = config["clustername"].as<string>();
        fmt::println("clustername: {}", clustername);
    } catch (...) {
        spdlog::error("No clustername definied");
        exit(1);
    }

    try {
        auto smtphost = config["smtphost"].as<string>();
        fmt::println("smtphost: {}", smtphost);
    } catch (...) {
        spdlog::warn("No smtphost found, beware: no reminder and error mails can be send");
    }

    try {
        auto mail_from = config["mail_from"].as<string>();
        fmt::println("mail_from: {}", mail_from);
    } catch (...) {
        spdlog::warn("No mail_from found, beware: no reminder and error mails can be send");
    }

    // default/default_workspace
    std::string defaultws;

    try {
        if (config["default"] && config["default_workspace"]){
            spdlog::error("Both 'default and 'default_workspace defined. Choose one.");
            exit(1);
        } else if (config["default"]){
            defaultws = config["default"].as<string>();
            fmt::println("default: {}", defaultws);
        } else if (config["default_workspace"]){
            defaultws = config["default_workspace"].as<string>();
            fmt::println("default_workspace: {}", defaultws);
        } else {
            spdlog::error("Neither 'default nor 'default_workspace' found.");
            exit(1);
        }
    } catch (...) {
        spdlog::error("No default found");
        exit(1);
    }
    // try if defaultws is actually in the ws
    if (!(std::find(wsnames.begin(), wsnames.end(), defaultws) != wsnames.end())){
        spdlog::error("default workspace is not defined as workspace in the file");
        exit(1);
    }

    // duration/maxduration 
    try {
        int duration;

        if (config["duration"] && config["maxduration"]) {
            spdlog::error("Both 'duration' and 'maxduration' defined, Choose one.");
            exit(1);
        } else if (config["duration"]){
            duration = config["duration"].as<int>();
            fmt::println("maxduration: {}", duration);
        } else if (config["maxduration"]){
            duration = config["maxduration"].as<int>();
            fmt::println("maxduration: {}", duration);
        } else {
            fmt::println("No duration found, continuing");
        }

    } catch (...) {
        fmt::println("No duration found, continuing");
    }

    try {
        auto durationdefault = config["durationdefault"].as<int>();
        fmt::println("durationdefault: {}", durationdefault);
    } catch (...) {
        spdlog::warn("No durationdefault found, defaults to 1 day, continuing");
    }

    try {
        auto maxextensions = config["maxextensions"].as<int>();
        fmt::println("maxextensions: {}", maxextensions);
    } catch (...) {
        spdlog::error("No maxextensions found, please add <\"maxextensions\": number> clause to toplevel");
    }

    try {
        auto dbuid = config["dbuid"].as<int>();
        fmt::println("dbuid: {}", dbuid);
    } catch (...) {
        spdlog::error("No dbuid defined, please add <\"dbuid\": uid> clause to toplevel");
        exit(1);
    }

    try {
        auto dbgid = config["dbgid"].as<int>();
        fmt::println("dbgid: {}", dbgid);
    } catch (...) {
        spdlog::error("No dbgid defined, please add <\"dbgid\": gid> clause to toplevel");
        exit(1);
    }

    try {
        auto admins = config["admins"].as<vector<string>>();
        fmt::println("admins: {}", admins);
    } catch (...) {
        fmt::println("No admins found, continuing");
    }

    try {
        auto adminmail = config["adminmail"].as<vector<string>>();
        fmt::println("adminmail: {}", adminmail);
    } catch (...) {
        spdlog::error("No adminmail found, please add <\"adminmail\": []> clause to toplevel");
        exit(1);
    }

    try {
        auto expirerlogpath = config["expirerlogpath"].as<string>();
        fmt::println("expirerlogpath: {}", expirerlogpath);
    } catch (...) {
        fmt::println("No expirerlogpath found, continuing");
    }

    try {
        auto deldirtimeout = config["deldirtimeout"].as<string>();
        fmt::println("deldirtimeout: {}", deldirtimeout);
    } catch (...) {
        fmt::println("No deldirtimeout found, continuing");
    }


    for (auto node : wslist){    
        for (auto it : node){
            spdlog::info("checking config for filesystem {}", it.first.as<string>());
            auto ws = it.second;
    
            try {
                auto keeptime = ws["keeptime"].as<int>();
                fmt::println("    keeptime: {}", keeptime);
            } catch (...) {
                fmt::println("    No keeptime found, continuing");
            }

            try {
                auto spaces = ws["spaces"].as<vector<string>>();
                fmt::println("    spaces: {}", spaces);
            } catch (...) {
                spdlog::error("No spaces found, please add <\"spaces\": []>");
            }
            
            try {
                auto spaceselection = ws["spaceselection"].as<string>();
                fmt::println("    spaceselection: {}", spaceselection);
            } catch (...) {
                fmt::println("    No spaceselection found, defaults to 'random', continuing");
            }

            try {
                auto deleted = ws["deleted"].as<string>();
                fmt::println("    deleted: {}", deleted);
            } catch (...) {
                spdlog::error("No deleted directory found, please add <\"deleted\": \"dir\"> clause to workspace");
            }

            try {
                auto database = ws["database"].as<string>();
                fmt::println("    workspace database directory: {}", database);
                if (!cppfs::exists(database)) {
                    spdlog::warn("database directory {} does not exist", database);
                }
                if (!cppfs::exists(database / cppfs::path(".ws_db_magic"))) {
                    spdlog::warn("database directory {} does not contain .ws_db_magic!", database);
                }
            } catch (...) {
                spdlog::error("No database location defined, please add <\"database\": \"dir\"> clause to workspace");
            }

            //todo this is for duration/maxduration 
            try {
                int duration;

                if (ws["duration"] && ws["maxduration"]){
                    spdlog::error("Both 'duration' and 'maxduration' defined. Choose one.");
                    exit(1);
                } else if (ws["duration"]){
                    duration = ws["duration"].as<int>();
                    fmt::println("    maxduration: {}", duration);
                } else if (ws["maxduration"]){
                    duration = ws["maxduration"].as<int>();
                    fmt::println("    maxduration: {}", duration);
                } else {
                    fmt::println("    No duration found, continuing");
                }
            } catch (...) {
                fmt::println("    No duration found, continuing");
            }

            try {
                auto groupdefault = ws["groupdefault"].as<vector<string>>();
                fmt::println("    groupdefault: {}", groupdefault);
            } catch (...) {
                fmt::println("    No groupdefault found, continuing");
            }

            try {
                auto userdefault = ws["userdefault"].as<vector<string>>();
                fmt::println("    userdefault: {}", userdefault);
            } catch (...) {
                fmt::println("    No userdefault found, continuing");
            }

            try {
                auto user_acl = ws["user_acl"].as<vector<string>>();
                fmt::println("    user_acl: {}", user_acl);
            } catch (...) {
                fmt::println("    No user_acl found, continuing");
            }

            try {
                auto group_acl = ws["group_acl"].as<vector<string>>();
                fmt::println("    group_acl: {}", group_acl);
            } catch (...) {
                fmt::println("    No group_acl found, continuing");
            }

            try {
                auto maxextensions = ws["maxextensions"].as<int>();
                fmt::println("    maxextensions: {}", maxextensions);
            } catch (...) {
                fmt::println("    No maxextensions found, continuing");
            }

            try {
                auto allocatable = ws["allocatable"].as<string>();
                fmt::println("    allocatable: {}", allocatable);
            } catch (...) {
                fmt::println("    No allocatable found, defaults to 'yes', continuing");
            }

            try {
                auto extendable = ws["extendable"].as<string>();
                fmt::println("    extendable: {}", extendable);
            } catch (...) {
                fmt::println("    No extendable found, defaults to 'yes', continuing");
            }

            try {
                auto restorable = ws["restorable"].as<int>();
                fmt::println("    restorable: {}", restorable);
            } catch (...) {
                fmt::println("    No restorable found, defaults to 'yes', continuing");
            }
        }
}
    spdlog::info("config is valid!");



}