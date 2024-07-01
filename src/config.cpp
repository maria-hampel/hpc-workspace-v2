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
#include <filesystem>

#include "yaml-cpp/yaml.h"
#include "fmt/base.h"
#include "fmt/ranges.h"

#include "config.h"
#include "db.h"
#include "dbv1.h"



extern bool debugflag;
extern bool traceflag;

std::string get_file_contents(const char *filename);

// read config from file or directory
Config::Config(cppfs::path filename) {
    // FIXME: check if file or directory
    string yaml = get_file_contents(filename.c_str());
    readYAML(yaml);
}

// read config from YAML node
Config::Config(std::string configstring) {
    readYAML(configstring);
}


void Config::readYAML(string yaml) {
    auto config = YAML::Load(yaml);
    // global flags
    bool valid = true;
    if (config["clustername"]) global.clustername = config["clustername"].as<string>();
    if (config["smtphost"]) global.smtphost = config["smtphost"].as<string>();
    if (config["mail_from"]) global.mail_from= config["mail_from"].as<string>();
    if (config["default_workspace"]) global.default_workspace = config["default_workspace"].as<string>();
    if (config["duration"]) global.duration = config["duration"].as<int>(); else global.duration = 30;
    if (config["reminderdefault"]) global.reminderdefault = config["reminderdefault"].as<int>();
    if (config["maxextensions"]) global.maxextensions = config["maxextensions"].as<int>(); else global.maxextensions = 100;
    if (config["dbuid"]) global.dbuid = config["dbuid"].as<int>(); else {fmt::print(stderr, "ERROR: no db uid in config\n"); valid=false;}
    if (config["dbgid"]) global.dbgid = config["dbgid"].as<int>(); else {fmt::print(stderr, "ERROR: no db gid in config\n"); valid=false;}
    if (config["admins"]) global.admins = config["admins"].as<vector<string>>();

    // SPEC:CHANGE accept filesystem as alias for workspaces
    if (config["workspaces"] || config["filesystems"]) {
        // auto list = config["workspaces"] ? config["workspaces"] : config["filesystems"];
        for(auto key: std::vector<string>{"workspaces","filesystems"}) {
            if (config[key]) { 

                auto list = config[key];

                for(auto it: list) {
                    Filesystem_config fs;
                    fs.name = it.first.as<string>();
                    if (debugflag) fmt::print(stderr, "debug: config, reading workspace {}\n", fs.name);
                    auto ws=it.second;
                    if (ws["spaces"]) fs.spaces = ws["spaces"].as<vector<string>>(); else {fmt::print(stderr, "ERROR: no spaces path for {}\n", fs.name); valid=false;}
                    if (ws["spaceselection"]) fs.spaceselection = ws["spaceselection"].as<string>(); else fs.spaceselection = "random";
                    if (ws["deleted"]) fs.deletedPath = ws["deleted"].as<string>(); else {fmt::print(stderr, "ERROR: no deleted path for {}\n", fs.name); valid=false;}
                    if (ws["database"]) fs.database = ws["database"].as<string>(); else {fmt::print(stderr, "ERROR: no database path for {}\n", fs.name); valid=false;}
                    if (ws["groupdefault"]) fs.groupdefault = ws["groupdefault"].as<vector<string>>();
                    if (ws["userdefault"]) fs.userdefault = ws["userdefault"].as<vector<string>>();
                    if (ws["user_acl"]) fs.user_acl = ws["user_acl"].as<vector<string>>();
                    if (ws["group_acl"]) fs.group_acl = ws["group_acl"].as<vector<string>>();
                    if (ws["keeptime"]) fs.keeptime = ws["keeptime"].as<int>(); else fs.keeptime = 10;
                    if (ws["maxduration"]) fs.maxduration = ws["maxduration"].as<int>();
                    if (ws["maxextensions"]) fs.maxextensions = ws["maxextensions"].as<int>();
                    if (ws["allocatable"]) fs.allocatable = ws["allocatable"].as<bool>(); else fs.allocatable = true;
                    if (ws["extendable"]) fs.extendable = ws["extendable"].as<bool>(); else fs.extendable = true;
                    if (ws["restorable"]) fs.restorable = ws["restorable"].as<bool>(); else fs.restorable = true;
                    filesystems[fs.name] = fs;
                }
            }
        }
    } else {
        fmt::print(stderr, "ERROR: no workspaces in config!\n"); valid=false;
    }

    if (!valid) {
        fmt::print(stderr, "ERROR: invalid config!\n");
        exit(-1);
    }
    // FIXME add unit test for validator
}

// is user admin?
bool Config::isAdmin(const string user) {
    return std::find(global.admins.begin(), global.admins.end(), user) != global.admins.end();
    // FIXME: add unit test
}



// check if given user can assess given filesystem with current config
//  see validFilesystems for specification of ACLs
// unittest: yes
bool Config::hasAccess(const string user, const vector<string> groups, const string filesystem) {
    bool ok = true;
    
    if (traceflag) fmt::print(stderr,"hasAccess({},{},{})",user,groups,filesystem);

    // see if FS is valid
    if ( filesystems.count(filesystem)<1 ) {
        fmt::print(stderr,"error: invalid filesystem queried for access: {}\n", filesystem);
        return false;
    }

    // check ACLs, group first, user second to allow -user to override group grant
    if (filesystems[filesystem].user_acl.size()>0 || filesystems[filesystem].group_acl.size()>0) {
        // as soon as any ACL is presents, access is denied and has to be granted
        ok = false;
        if (debugflag) fmt::print(stderr,"  ACL present, access denied\n");

        if (filesystems[filesystem].group_acl.size()>0) {
            if (debugflag) fmt::print(stderr, "    group ACL present,");
            for(const auto & group : groups) {
                if (canFind(filesystems[filesystem].group_acl, group)) ok = true;
                if (canFind(filesystems[filesystem].group_acl, string("+")+group)) ok = true;
                if (canFind(filesystems[filesystem].group_acl, string("-")+group)) ok = false;
                if (debugflag) fmt::print(stderr, "    access {}\n", ok?"granted":"denied");
            }
        }

        if (filesystems[filesystem].user_acl.size()>0) {
            if (debugflag) fmt::print(stderr, "    user ACL present,");
            if (canFind(filesystems[filesystem].user_acl, user)) ok = true;
            if (canFind(filesystems[filesystem].user_acl, string("+")+user)) ok = true;
            if (canFind(filesystems[filesystem].user_acl, string("-")+user)) ok = false;
            if (debugflag) fmt::print(stderr,"    access {}\n", ok?"granted":"denied");
        }
    }

    // check admins list, admins can see and access all filesystems
    if (global.admins.size()>0) {
        if (debugflag) fmt::print(stderr, "    admin list present, ");
        if (canFind(global.admins, user)) ok = true;
        if (debugflag) fmt::print(stderr,"    access {}\n", ok?"granted":"denied");
    }

    if (debugflag) fmt::print(stderr," => access to <{}> for user <{}> {}", filesystem, user, ok?"granted":"denied");

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
vector<string> Config::validFilesystems(const string user, const vector<string> groups) {
        vector<string> validfs;

        if (debugflag) {
            // avoid vector<> fmt::print for clang <=17 at least
            fmt::print(stderr, "validFilesystems({},{}) over ",user, groups);
            for(const auto &[key, value]: filesystems) fmt::print(key);
            fmt::print("\n");
        }

        if ((global.default_workspace != "") && hasAccess(user, groups, global.default_workspace) ) {
            if (debugflag) fmt::print(stderr,"  adding default_workspace <{}>\n", global.default_workspace);
            validfs.push_back(global.default_workspace);
        }

        // check if group or user default, user first
        // SPEC: with users first a workspace with user default is always in front of a groupdefault
        for(auto const &[fs,val]: filesystems) {
            if (canFind(filesystems[fs].userdefault, user)) {
                if (debugflag) fmt::print(stderr, "  checking if userdefault <{}> already added\n", fs);
                if (hasAccess(user, groups, fs) && !canFind(validfs, fs)) {
                    if (debugflag) fmt::print(stderr,"    adding userdefault <{}>\n", fs);
                    validfs.push_back(fs);
                }
            }
        }

        // now groups
        for(auto const &[fs,val]: filesystems) {
            for(string group: groups) {
                if (debugflag) fmt::print(stderr,"  checking if groupdefault <{}> already added\n", fs);
                if (canFind(filesystems[fs].groupdefault, group)) {
                    if (hasAccess(user, groups, fs) && !canFind(validfs, fs)) {
                        if (debugflag) fmt::print(stderr,"    adding groupdefault <{}>\n", fs);
                        validfs.push_back(fs);
                    }
                }
            }
        }

        // now again all with access
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
Database* Config::openDB(const string fs) {
    if (traceflag) fmt::print("opendb {}\n", fs);
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

// returnpath to deletedpath for given filesystem, or empty string
string Config::deletedPath(const string filesystem) const {
    auto it = filesystems.find(filesystem);
    if (it == filesystems.end()) 
        return string("");
    else
        return it->second.deletedPath;  
}



