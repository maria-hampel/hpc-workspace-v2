/*
 *  hpc-workspace-v2
 *
 *  config.cpp
 * 
 *  - deals with all configuration files, global or user local
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


#include <vector>
#include <algorithm>

#include "yaml-cpp/yaml.h"
#include "fmt/base.h"
#include "fmt/ranges.h"

#include "config.h"
#include "db.h"
#include "dbv1.h"
#include "utils.h"

#include <iostream>
#include <string>
#include <sstream>

#include <filesystem>
namespace cppfs = std::filesystem;

extern bool debugflag;
extern bool traceflag;


// tries to read a list of config files, in given order, can be used to check for /etc/ws.d first and /etc/ws.conf second
// stops when file can be read, but reads all files in case of directory given
Config::Config(const std::vector<cppfs::path> configpathes) {
    // some defaults
    global.dbuid = 0;
    global.dbgid = 0;
    global.maxextensions = 10;
    global.duration = 100;
    global.durationdefault = 30;
    global.reminderdefault = 0;

    bool filefound = false;

    for(const auto &configpath: configpathes) {
        if (cppfs::exists(configpath)) {
            filefound = true;
            if (cppfs::is_regular_file(configpath)) {
                if (debugflag) fmt::println("Info   : Reading config file {}", configpath.string());
                string yaml = utils::getFileContents(configpath);
                readYAML(yaml);
            } else if (cppfs::is_directory(configpath)) {
                if (debugflag) fmt::println("Info   : Reading config directory {}", configpath.string());

                // sort pathes
                std::vector<std::string> pathesToSort;
                for (const auto& entry : cppfs::directory_iterator(configpath)) {
                    pathesToSort.push_back(entry.path().string());
                }
                std::sort(pathesToSort.begin(), pathesToSort.end());

                for(const auto &cfile: pathesToSort) {
                    if (cppfs::is_regular_file(cfile)) {
                        if (debugflag) fmt::println("Info   : Reading config file {}", cfile);
                        string yaml = utils::getFileContents(cfile);
                        readYAML(yaml);                    
                    } 
                }

            } else {
                fmt::println(stderr, "Info   : Unexpected filetype of {}", configpath.string());
                exit(-1); // bail out, someone is messing around
            }
            break; // stop after first file
        }
    }

    if (!filefound) {
        isvalid = false;
        fmt::println(stderr, "Error  : None of the config file exists!");
        return;
    }
    validate();

}


// read config from YAML node
Config::Config(const std::string configstring) {
    readYAML(configstring);
}

// validate config, return false if invalid 
bool Config::validate() {
    bool valid = true;
    if (global.dbuid==0) {
        valid = false;
        fmt::println(stderr, "Error  : No dbuid in config!");
    }
    if (global.dbgid==0) {
        valid = false;
        fmt::println(stderr, "Error  : No dbgid in config!");
    }
    if (global.clustername.empty()) {
        valid = false;
        fmt::println(stderr, "Error  : No clustername in config!");
    }
    if (global.adminmail.empty()) {
        valid = false;
        fmt::println(stderr, "Error  : No adminmail in config!");
    }    
    // SPEC:CHANGE: require default workspace
    if (global.default_workspace.empty()) {
        valid = false;
        fmt::println(stderr, "Error  : No default filesystem in config!");
    }    
    if (filesystems.empty()) {
        valid = false;
        fmt::println(stderr, "Error  : No filesystems in config!");        
    }

    for (auto const &[fsname, fsdata]: filesystems) {
        if (fsdata.spaces.empty()) {
            valid = false;
            fmt::println(stderr, "Error  : No spaces in filesystem <> in config!", fsname);      
        }
        if (fsdata.database.empty()) {
            valid = false;
            fmt::println(stderr, "Error  : No database path in filesystem <> in config!", fsname);      
        }
        if (fsdata.deletedPath.empty()) {
            valid = false;
            fmt::println(stderr, "Error  : No deleted name in filesystem <> in config!", fsname);      
        }
    }
    isvalid = valid;
    return valid;
}

// parse YAML from a string (using yaml-cpp)
void Config::readYAML(const string yaml) {
    auto config = YAML::Load(yaml);
    // global flags
    if (config["clustername"]) global.clustername = config["clustername"].as<string>();
    if (config["smtphost"]) global.smtphost = config["smtphost"].as<string>();
    if (config["mail_from"]) global.mail_from= config["mail_from"].as<string>();
    if (config["default_workspace"]) global.default_workspace = config["default_workspace"].as<string>();  // SPEC:CHANGE accept alias default_workspace
    if (config["default"]) global.default_workspace = config["default"].as<string>();
    if (config["duration"]) global.duration = config["duration"].as<int>(); 
    if (config["durationdefault"]) global.durationdefault = config["durationdefault"].as<int>(); 
    if (config["reminderdefault"]) global.reminderdefault = config["reminderdefault"].as<int>(); 
    if (config["maxextensions"]) global.maxextensions = config["maxextensions"].as<int>(); 
    if (config["dbuid"]) global.dbuid = config["dbuid"].as<int>(); 
    if (config["dbgid"]) global.dbgid = config["dbgid"].as<int>(); 
    if (config["admins"]) global.admins = config["admins"].as<vector<string>>();
    if (config["adminmail"]) global.adminmail = config["admins"].as<vector<string>>();

    // SPEC:CHANGE accept filesystem as alias for workspaces to better match the -F option of the tools
    if (config["workspaces"] || config["filesystems"]) {
        for(auto key: std::vector<string>{"workspaces","filesystems"}) {
            if (config[key]) { 

                auto list = config[key];

                for(auto it: list) {
                    Filesystem_config fs;
                    fs.name = it.first.as<string>();
                    if (debugflag) fmt::print(stderr, "debug: config, reading workspace {}\n", fs.name);
                    auto ws=it.second;
                    if (ws["spaces"]) fs.spaces = ws["spaces"].as<vector<string>>(); 
                    if (ws["spaceselection"]) fs.spaceselection = ws["spaceselection"].as<string>(); else fs.spaceselection = "random";
                    if (ws["deleted"]) fs.deletedPath = ws["deleted"].as<string>(); 
                    if (ws["database"]) fs.database = ws["database"].as<string>();
                    if (ws["groupdefault"]) fs.groupdefault = ws["groupdefault"].as<vector<string>>();
                    if (ws["userdefault"]) fs.userdefault = ws["userdefault"].as<vector<string>>();
                    if (ws["user_acl"]) fs.user_acl = ws["user_acl"].as<vector<string>>();
                    if (ws["group_acl"]) fs.group_acl = ws["group_acl"].as<vector<string>>();
                    if (ws["keeptime"]) fs.keeptime = ws["keeptime"].as<int>(); else fs.keeptime = 10;
                    if (ws["maxduration"]) fs.maxduration = ws["maxduration"].as<int>(); else fs.maxduration = 0;
                    if (ws["maxextensions"]) fs.maxextensions = ws["maxextensions"].as<int>(); else fs.maxextensions = 0;
                    if (ws["allocatable"]) fs.allocatable = ws["allocatable"].as<bool>(); else fs.allocatable = true;
                    if (ws["extendable"]) fs.extendable = ws["extendable"].as<bool>(); else fs.extendable = true;
                    if (ws["restorable"]) fs.restorable = ws["restorable"].as<bool>(); else fs.restorable = true;
                    filesystems[fs.name] = fs;
                }
            }
        }
    }
}

// is user admin?
// unittest: yes
bool Config::isAdmin(const string user) {
    return std::find(global.admins.begin(), global.admins.end(), user) != global.admins.end();
}



// check if given user can assess given filesystem with current config
//  see validFilesystems for specification of ACLs
// unittest: yes
bool Config::hasAccess(const string user, const vector<string> groups, const string filesystem) const {
    if(traceflag) fmt::print(stderr, "Trace  : hasAccess(user={},groups={},filesystem={})\n", user, groups, filesystem);

    bool ok = true;

    // see if FS is valid
    if ( filesystems.count(filesystem)<1 ) {
        fmt::print(stderr,"Error  : invalid filesystem queried for access: {}\n", filesystem);
        return false;
    }

    // check ACLs, group first, user second to allow -user to override group grant
    if (filesystems.at(filesystem).user_acl.size()>0 || filesystems.at(filesystem).group_acl.size()>0) {
        // as soon as any ACL is presents, access is denied and has to be granted
        ok = false;
        if (debugflag) fmt::print(stderr,"  ACLs present\n");

        if (filesystems.at(filesystem).group_acl.size()>0) {
            if (debugflag) fmt::print(stderr, "    group ACL present,");
            for(const auto & group : groups) {
                if (canFind(filesystems.at(filesystem).group_acl, group)) ok = true;
                if (canFind(filesystems.at(filesystem).group_acl, string("+")+group)) ok = true;
                if (canFind(filesystems.at(filesystem).group_acl, string("-")+group)) ok = false;
                if (debugflag) fmt::print(stderr, "    access for {} {}\n", group, ok?"granted":"denied");
            }
        }

        if (filesystems.at(filesystem).user_acl.size()>0) {
            if (debugflag) fmt::print(stderr, "    user ACL present,");
            if (canFind(filesystems.at(filesystem).user_acl, user)) ok = true;
            if (canFind(filesystems.at(filesystem).user_acl, string("+")+user)) ok = true;
            if (canFind(filesystems.at(filesystem).user_acl, string("-")+user)) ok = false;
            if (debugflag) fmt::print(stderr,"    access for {} {}\n", user, ok?"granted":"denied");
        }
    }

    // check admins list, admins can see and access all filesystems
    if (global.admins.size()>0) {
        if (debugflag) fmt::print(stderr, "    admin list present, ");
        if (canFind(global.admins, user)) ok = true;
        if (debugflag) fmt::print(stderr,"    access {}\n", ok?"granted":"denied");
    }

    if (debugflag) fmt::print(stderr," => access to <{}> for user <{}> {}\n", filesystem, user, ok?"granted":"denied");

    return ok;

}


// get list of valid filesystems for given user, each filesystem is only once in the list
//  SPEC: validFilesystems(user, groups)
//  SPEC: this list is sorted: userdefault, groupdefault, global default, others
//  SPEC:CHANGE: a user has to be able to access global default filesystem, otherwise it will be not returned here 
//  SPEC:CHANGE: a user or group acl can contain a username with - prefixed, to disallow access  
//  SPEC:CHANGE: a user or group acl can contain a username with + prefix, to allow access, same as only listing user/group
//  SPEC: as soon as an ACL exists, access is denied to those not in ACL
//  SPEC: user acls are checked after groups for - entries, so users can be excluded after having group access
//  SPEC:CHANGE: a user default does NOT override an ACL
//  SPEC: admins have access to all filesystems
// unittest: yes
vector<string> Config::validFilesystems(const string user, const vector<string> groups) const {
        vector<string> validfs;

        if (debugflag) {
            // avoid vector<> fmt::print for clang <=17 at least
            fmt::print(stderr, "validFilesystems({},{}) over ",user, groups);
            for(const auto &[fs, val]: filesystems) fmt::print("{} ", fs);
            fmt::print("\n");
        }

        // check if group or user default, user first
        // SPEC: with users first a workspace with user default is always in front of a groupdefault
        for(auto const &[fs,val]: filesystems) {
            if (debugflag) fmt::print("fs={} filesystems.at(fs).userdefault={}\n", fs, filesystems.at(fs).userdefault);
            if (canFind(filesystems.at(fs).userdefault, user)) {
                if (debugflag) fmt::print(stderr, "  checking if userdefault <{}> already added\n", fs);
                if (hasAccess(user, groups, fs) && !canFind(validfs, fs)) {
                    if (debugflag) fmt::print(stderr,"    adding userdefault <{}>\n", fs);
                    validfs.push_back(fs);
                    break;
                }
            }
        }

        // now groups
        for(auto const &[fs,val]: filesystems) {
            for(string group: groups) {
                if (debugflag) fmt::print(stderr,"  checking if group <{}> in groupdefault[{}]={}\n", group, fs, filesystems.at(fs).groupdefault);
                if (canFind(filesystems.at(fs).groupdefault, group)) {
                    if (hasAccess(user, groups, fs) && !canFind(validfs, fs)) {
                        if (debugflag) fmt::print(stderr,"    adding groupdefault <{}>\n", fs);
                        validfs.push_back(fs);
                        goto groupend;
                    }
                }
            }
        }
        groupend:

        // global default last
        if (debugflag) {
            fmt::println(stderr, "global.default_workspace={}", global.default_workspace);
            fmt::println(stderr, "hasAccess({}, {}, {})={}", user, groups, global.default_workspace,hasAccess(user, groups, global.default_workspace));
            fmt::println(stderr, "canFind({}, {})={}", validfs, global.default_workspace, canFind(validfs, global.default_workspace));
        }
        if ((global.default_workspace != "") && hasAccess(user, groups, global.default_workspace) && !canFind(validfs, global.default_workspace) ) {
            if (debugflag) fmt::print(stderr,"  adding default_workspace <{}>\n", global.default_workspace);
            validfs.push_back(global.default_workspace);
        }

        // now all others with access
        for(auto const &[fs,val]: filesystems) {
            if (hasAccess(user, groups, fs) && !canFind(validfs, fs)) {
                if (debugflag) fmt::print(stderr,"    adding as having access <{}>\n", fs);
                validfs.push_back(fs);
            }
        }

        if (debugflag) fmt::print(stderr," => valid filesystems {}\n", validfs);

        return validfs;
}

// get DB type for the fs 
Database* Config::openDB(const string fs) const {
    if (traceflag) fmt::print(stderr, "Trace  : opendb {}\n", fs);
    // FIXME: version check here to determine which DB to open
    return new FilesystemDBV1(this, fs);
}

// return path to database for given filesystem, or empy string
string Config::database(const string filesystem) const {
    auto it = filesystems.find(filesystem);
    if (it == filesystems.end()) 
        return string("");
    else
        return it->second.database;
}

// return path to deletedpath for given filesystem, or empty string
string Config::deletedPath(const string filesystem) const {
    auto it = filesystems.find(filesystem);
    if (it == filesystems.end()) 
        return string("");
    else
        return it->second.deletedPath;  
}

// return config of filesystem throw if invalid
Filesystem_config Config::getFsConfig(const std::string filesystem) const {
    if(traceflag) fmt::print(stderr, "Trace  : getFsConfig({})\n", filesystem);
    try {
        return filesystems.at(filesystem);
    } catch (const std::out_of_range &e) {
        fmt::print(stderr, "no valid filesystem ({}) given in getFsConfig(), should not happen\n", filesystem);
        exit(-1); // should not be reached
    }
}


// read user config from string (has to be read before dropping privileges)
UserConfig::UserConfig(std::string userconf) {    
    YAML::Node user_home_config;  // load yaml file from home here, not used anywhere else so far
    // get first line, this is either a mailaddress or something like key: value
    // std::getline(userconf, mailaddress);
    // check if file looks like yaml
    if (userconf.find(":",0) != string::npos) {
        user_home_config = YAML::Load(userconf);
        try {
            mailaddress = user_home_config["mail"].as<std::string>();
            // FIXME: validate mail address
        } catch (...) {
            mailaddress = "";
        }
    } else {
        mailaddress = userconf;
    }
}