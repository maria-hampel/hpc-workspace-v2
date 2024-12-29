/*
 *  hpc-workspace-v2
 *
 *  ws_release
 *
 *  - tool to release workspaces
 * 
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *
 *  (c) Holger Berger 2021,2023,2024
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

#include <iostream>
#include <string>
#include "fmt/base.h"
#include "fmt/ranges.h"

#include <regex> // buggy in redhat 7

#include <syslog.h>

#include "build_info.h"
#include "config.h"
#include "user.h"
#include "utils.h"
#include "caps.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;
using namespace std;

#include <filesystem>
namespace cppfs = std::filesystem;

// global variables

bool debugflag = false;
bool traceflag = false;

// init caps here, when euid!=uid
Cap caps{};




/* 
 *  parse the commandline and see if all required arguments are passed, and check the workspace name for 
 *  bad characters
 */
void commandline(po::variables_map &opt, string &name, string &filesystem,
                    string &user, string &groupname, bool &deletedata,
                    int argc, char**argv, std::string &userconf, std::string &configfile) {
    // define all options

    po::options_description cmd_options( "\nOptions" );
    cmd_options.add_options()
            ("help,h", "produce help message")
            ("version,V", "show version")
            ("name,n", po::value<string>(&name), "workspace name")
            ("filesystem,F", po::value<string>(&filesystem), "filesystem")
            ("username,u", po::value<string>(&user), "username")
            ("groupname,G", po::value<string>(&groupname)->default_value(""), "groupname")
            ("config,c", po::value<string>(&configfile), "config file")
            ("delete-data", "delete all data, workspace can NOT BE RECOVERED!")
    ;

    po::options_description secret_options("Secret");
    secret_options.add_options()
        ("debug", "show debugging information")
        ("trace", "show calling information")
        ;

    // define options without names
    po::positional_options_description p;
    p.add("name", 1).add("duration",2);

    po::options_description all_options;
    all_options.add(cmd_options).add(secret_options);

    // parse commandline
    try{
        po::store(po::command_line_parser(argc, argv).options(all_options).positional(p).run(), opt);
        po::notify(opt);
    } catch (...) {
        fmt::print("Usage: {} [options] workspace_name duration\n", argv[0] );
        cout << cmd_options << "\n";
        exit(1);
    }

    // see whats up

    if (opt.count("help")) {
        fmt::print("Usage: {} [options] workspace_name duration\n", argv[0]);
        cout << cmd_options << "\n";
        exit(1);
    }

    if (opt.count("version")) {
#ifdef IS_GIT_REPOSITORY
        fmt::println("workspace build from git commit hash {} on top of release {}", GIT_COMMIT_HASH, WS_VERSION);
#else
        fmt::println("workspace version {}", WS_VERSION );
#endif
        utils::printBuildFlags();
        exit(1);
    }

    // this allows user to extend foreign workspaces
    if(opt.count("username") && !( opt.count("extension") || getuid()==0 ) ) {
        fmt::print(stderr, "Info   : Ignoring username option.\n");
        user="";
    }

    if (opt.count("name"))
    {
        //cout << " name: " << name << "\n";
    } else {
        fmt::print("{}: [options] workspace_name duration\n", argv[0]);
        cout << cmd_options << "\n"; // FIXME: iostream usage
        exit(1);
    }

    deletedata = opt.count("deletedata");

    // globalflags
    debugflag = opt.count("debug");
    traceflag = opt.count("trace");

    // validate workspace name against nasty characters    
    //  static const std::regex e("^[a-zA-Z0-9][a-zA-Z0-9_.-]*$");  // #77
    // TODO: remove regexp dependency
    static const regex e("^[[:alnum:]][[:alnum:]_.-]*$");
    if (!regex_match(name, e)) {
            fmt::print(stderr, "Error  : Illegal workspace name, use ASCII characters and numbers, '-','.' and '_' only!");
            exit(1);
    }
}


/*
 *  validate parameters vs config file
 *
 *  return true if ok and false if not
 * 
 *  changes duration and maxextensions, does return true if they are out of bounds
 */
bool validateFsAndGroup(const Config &config, const po::variables_map &opt, const std::string username)
{
    if (traceflag) fmt::print(stderr, "Trace  : validateFsAndGroup(username={})", username);

    //auto groupnames=getgroupnames(username); // FIXME:  use getGrouplist ?
    auto groupnames = user::getGrouplist();

    // if a group was given, check if a valid group was given
    if ( opt["groupname"].as<string>() != "" ) {
        if ( find(groupnames.begin(), groupnames.end(), opt["groupname"].as<string>()) == groupnames.end() ) {
            fmt::print(stderr, "Error  : invalid group specified!\n");
            return false;
        }
    }

    // if the user specifies a filesystem, he must be allowed to use it
    if(opt.count("filesystem")) {
        auto validfs = config.validFilesystems(username, groupnames);
        if ( !canFind(validfs, opt["filesystem"].as<string>()) && getuid()!=0 ) {
            fmt::print(stderr, "Error  : You are not allowed to use the specified filesystem!\n");
            return false;
        }
    } 

    return true;
}




/*
 *  release the workspace
 *  file accesses and config access are hidden in DB and config handling
 *  FIXME: make it -> int and return errors for tesing
 */
void release(  
            const Config &config,
            const po::variables_map &opt, string filesystem, const string name,
            string user_option, const string groupname, const bool deletedata
            ) 
{
    if (traceflag) fmt::print(stderr, "Trace  : releae({}, {}, {}, {}, {})\n", filesystem, name,
                                user_option, groupname, deletedata);


    std::string username = user::getUsername(); // current user

    // get valid filesystems to bail out if there is none
    auto valid_filesystems = config.validFilesystems(user::getUsername(), user::getGrouplist());

    if (valid_filesystems.size()==0) {
        fmt::print(stderr, "Error: no valid filesystems in configuration, can not allocate\n");
        exit(-1); // FIXME: bad for testing
    }

    // validate filesystem and group given on command line 
    if (!validateFsAndGroup(config, opt, user_option )) {
        fmt::print(stderr, "Error: aborting!\n");
    }
  

    // if no filesystem provided, get valid filesystems from config, ordered: userdefault, groupdefault, globaldefault, others
    vector<string> searchlist;
    if(opt.count("filesystem")) {
        searchlist.push_back(opt["filesystem"].as<string>());
    } else {
        searchlist = config.validFilesystems(user::getUsername(), user::getGrouplist());  // FIXME: getUsername or user_option? getGrouplist uses current uid
    }

    //
    // now search for workspace in filesystem(s) and see if it exists or create it
    //

    bool ws_exists = false;
    std::string foundfs;
    
    std::unique_ptr<DBEntry> dbentry;
    std::string dbid;

    // loop over valid workspaces, and see if the workspace exists

    for (std::string cfilesystem: searchlist) {
        if (debugflag) {
            fmt::print(stderr, "Debug  : searching valid filesystems, currently {}\n", cfilesystem);
        }

        //auto db = config.openDB(cfilesystem);
        std::unique_ptr<Database> db(config.openDB(cfilesystem));

        // check if entry exists
        try {
            if(user_option.length()>0 && (getuid()==0)) 
                dbid = user_option+"-"+name;
            else 
                dbid = username+"-"+name;

            //dbentry = db->readEntry(dbid, false);
            dbentry = std::unique_ptr<DBEntry>(db->readEntry(dbid, false));
            foundfs = cfilesystem;
            ws_exists = true;
            break;
        } catch (DatabaseException &e) {
            // silently ignore non existiong entries
            if (debugflag) fmt::print(stderr, "Debug  :  existence check failed for {}/{}\n", cfilesystem, dbid);
        }   
    } // searchloop

    // workspace exists, release it

    if(ws_exists) {
        auto wsdir = dbentry->getWSPath();
 
        fmt::print(stderr, "IMPLEMENT ME");

    // if ws_exist
    } else {
        // workspace does not exist and needs to be created

        fmt::print(stderr, "Error  : Non-existent workspace given.\n");
    }

}



int main(int argc, char **argv) {
    string name;
    string filesystem;
    string mailaddress("");
    string user_option, groupname;
    string configfile;
    bool deletedata;
    po::variables_map opt;


    // locals settiongs to prevent strange effects
    utils::setCLocal();

    // find which config files to read
    //   user can change this if no setuid installation OR if root
    auto configfilestoread = std::vector<cppfs::path>{"/etc/ws.d","/etc/ws.conf"}; 
    if (configfile != "") {
        if (user::isRoot() || caps.isUserMode()) {
            configfilestoread = {configfile};
        } else {
            fmt::print(stderr, "Warning: ignored config file option!\n");
        }
    }

    // read the config
    auto config = Config(configfilestoread);
    if (!config.isValid()) {
        fmt::println(stderr, "Error  : No valid config file found!");
        exit(-2);
    }

    int db_uid = config.dbuid();

    // FIXME: make sure we are user here, not root!

    // lower capabilities to user, before interpreting any data from user
    caps.drop_caps({CAP_DAC_OVERRIDE, CAP_CHOWN, CAP_FOWNER}, getuid(), utils::SrcPos(__FILE__, __LINE__, __func__));
    // TODO:: moved up, check and may be fix in ws_allocate as well

    // read user config before dropping privileges 
    //std::stringstream user_conf;
    std::string user_conf;
    string user_conf_filename = user::getUserhome()+"/.ws_user.conf";
    if (!cppfs::is_symlink(user_conf_filename)) {
        if (cppfs::is_regular_file(user_conf_filename)) {
            user_conf = utils::getFileContents(user_conf_filename.c_str());
        }
        // FIXME: could be parsed here and passed as object not string
    } else {
        fmt::print(stderr,"Error  : ~/.ws_user.conf can not be symlink!");
        exit(-1);
    }



    // check commandline, get flags which are used to create ws object or for workspace release
    commandline(opt, name, filesystem, user_option, groupname, deletedata, argc, argv, user_conf, configfile);

    openlog("ws_allocate", 0, LOG_USER); // SYSLOG


    // FIXME:: which privileges here? user or dbuid?

    // release workspace
    release(config, opt, filesystem, name, user_option, groupname, deletedata);

}



