/*
 *  hpc-workspace-v2
 *
 *  dbv1.cpp
 * 
 *  - interface to v1 format database 
 *    this is the DB format as written from legacy workspace++
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


#include <string>
#include <vector>
#include <filesystem>

#define RAPIDYAML

#ifdef RAPIDYAML
    #define RYML_USE_ASSERT 0 
    #include "ryml.hpp"
    #include "ryml_std.hpp" 
    #include "c4/format.hpp" 
#else
    #include <yaml-cpp/yaml.h>
#endif

#include "dbv1.h"
#include "utils.h"
#include "fmt/core.h"
#include "fmt/ranges.h"

using namespace std;

extern bool debugflag;
extern bool traceflag;

namespace cppfs = std::filesystem;


vector<WsID> FilesystemDBV1::matchPattern(const string pattern, const string filesystem, const string user, const vector<string> groups,
                                                const bool deleted, const bool groupworkspaces)
{
    if(traceflag) fmt::print("matchPattern({},{},{},{},{},{})\n",pattern, filesystem, user, groups, deleted,groupworkspaces);

    string filepattern;

    // list directory, this also reads YAML file in case of groupworkspaces
    auto listdir = [&groupworkspaces, &groups] (const string pathname, const string filepattern) -> vector<string> {
        if(debugflag) fmt::print("listdir({},{})\n", pathname, filepattern);
        // in case of groupworkspace, read entry
        if (groupworkspaces) {
            auto filelist = dirEntries(pathname, filepattern);
            vector<string> list;
            for(auto const &f: filelist) {  
#ifndef RAPIDYAML
                YAML::Node dbentry;
                try {
                    dbentry = YAML::LoadFile((cppfs::path(pathname) / f).string().c_str());
                } catch (const YAML::BadFile& e) {
                    fmt::print(stderr,"Error: Could not read db entry {}: {}", f, e.what());
                }

                string group = dbentry["group"].as<string>();
#else
                string filecontent = get_file_contents((cppfs::path(pathname) / f).string().c_str());
                ryml::Tree dbentry = ryml::parse_in_place(ryml::to_substr(filecontent));                // FIXME: error check?

                ryml::NodeRef node;
                string group;
                node=dbentry["group"]; if(node.has_val()&& node.val()!="") node>>group; else group = "";
#endif

                if (canFind(groups, group)) {
                        list.push_back(f);
                }
            }         
            return list;   
        } else {
            return dirEntries(pathname, filepattern);
        }
    };

    // this has to happen here, as other DB might have different patterns
    if (groupworkspaces)
            filepattern = fmt::format("*-{}",pattern);
    else
            filepattern = fmt::format("{}-{}", user,pattern);

    // scan filesystem
    if (deleted)
            return listdir(cppfs::path(config->database(filesystem)) / config->deletedPath(filesystem), filepattern);
    else
            return listdir(config->database(filesystem), filepattern);


}


// read entry
DBEntry* FilesystemDBV1::readEntry(const string filesystem, const WsID id, const bool deleted) {
    if(traceflag) fmt::print("readEntry({},{},{})\n", filesystem, id, deleted);
    auto *entry = new DBEntryV1;
    string filename;
    if (deleted) 
        filename = cppfs::path(config->database(filesystem)) / config->deletedPath(filesystem) / id;
    else
        filename = cppfs::path(config->database(filesystem)) / id;
    entry->readFromFile(id, filesystem, filename);

    return entry;
}



#ifndef RAPIDYAML

// read db entry from yaml file
//  throws on error
void DBEntryV1::readFromFile(const WsID id, const string filesystem, const string filename) {
    if(traceflag) fmt::print("readFromFile_YAMLCPP({},{},{})\n", id, filesystem, filename);

    YAML::Node dbentry;

    try {
        dbentry = YAML::LoadFile(filename);
    } catch (const YAML::BadFile& e) {
        fmt::print(stderr,"Error: Could not read db entry {}: {}", filename, e.what());
        // FIXME: throw something here
    }

    dbversion = dbentry["dbversion"] ? dbentry["dbversion"].as<int>() : 0;   // 0 = legacy
    this->id = id;
    this->filesystem = filesystem;
    creation = dbentry["creation"] ? dbentry["creation"].as<long>() : 0;  // FIXME: c++ tool does not write this field, but takes from stat
    released = dbentry["released"] ? dbentry["released"].as<long>() : 0;
    expiration = dbentry["expiration"] ? dbentry["expiration"].as<long>() : 0;
    reminder = dbentry["reminder"] ? dbentry["reminder"].as<long>() : 0;
    workspace = dbentry["workspace"] ? dbentry["workspace"].as<string>() : "";
    extensions = dbentry["extensions"] ? dbentry["extensions"].as<int>() : 0;
    mailaddress = dbentry["mailaddress"] ? dbentry["mailaddress"].as<string>() : "";
    comment = dbentry["comment"] ? dbentry["comment"].as<string>() : "";
    group = dbentry["group"] ? dbentry["group"].as<string>() : ""; 
}

#else

// read db entry from yaml file
//  throws on error
void DBEntryV1::readFromFile(const WsID id, const string filesystem, const string filename) {
    if(traceflag) fmt::print("readFromFile_RAPIDYAML({},{},{})\n", id, filesystem, filename);

    string filecontent = get_file_contents(filename.c_str());
    if(filecontent=="") {
        fmt::print(stderr,"Error: Could not read db entry {}", filename);
        // FIXME: throw something here
    }

    ryml::Tree dbentry = ryml::parse_in_place(ryml::to_substr(filecontent));  // FIXME: error check?

    ryml::NodeRef node;

    node=dbentry["dbversion"]; if(node.has_val()) node>>dbversion; else dbversion = 0;  // 0 = legacy
    this->id = id;
    this->filesystem = filesystem;
    node=dbentry["creation"]; if(node.has_val()) node>>creation; else creation = 0;  // FIXME: c++ tool does not write this field, but takes from stat
    node=dbentry["released"]; if(node.has_val()) node>>released; else released = 0;
    node=dbentry["expiration"]; if(node.has_val()) node>>expiration; else expiration = 0;
    node=dbentry["reminder"]; if(node.has_val()) node>>reminder; else reminder = 0;
    node=dbentry["workspace"]; if(node.has_val() && node.val()!="") node>>workspace; else workspace = "";
    node=dbentry["extensions"]; if(node.has_val()) node>>extensions; else extensions = 0;
    node=dbentry["mailaddress"]; if(node.has_val() && node.val()!="") node>>mailaddress; else mailaddress = "";
    node=dbentry["comment"]; if(node.has_val() && node.val()!="") node>>comment; else comment = "";
    node=dbentry["group"]; if(node.has_val() && node.val()!="") node>>group; else group = "";
}

#endif



// print entry to stdout, for ws_list
void DBEntryV1::print(const bool verbose, const bool terse) {
    string repr;
    long remaining = expiration - time(0L);

    fmt::print(
        "Id: {}\n"
        "    workspace directory  : {}\n",
        id, workspace);
if (remaining<0) {
        fmt::print("    remaining time       : {}\n", "expired");
    } else {
        fmt::print("    remaining time       : {} days, {} hours\n", remaining/(24*3600), (remaining%(24*3600))/3600);
    }
    if(!terse) {
        if(comment!="")
            fmt::print("    comment              : {}\n", comment);
        if (creation>0)
            fmt::print("    creation time        : {}", ctime(&creation));
        fmt::print("    expiration time      : {}", ctime(&expiration));
        fmt::print("    filesystem name      : {}\n", filesystem);
    }
    fmt::print("    available extensions : {}\n", extensions);
    if (verbose) {
        long rd = expiration - reminder/(24*3600);
        fmt::print("    reminder             : {}", ctime(&rd));
        fmt::print("    mailaddress          : {}\n", mailaddress);
    }
};


long DBEntryV1::getRemaining() {
    return expiration - time(0L);
};
string DBEntryV1::getId() {
    return id;
}
long DBEntryV1::getCreation() {
    return creation;
}
string DBEntryV1::getWSPath() {
    return workspace;
};
		