/*
 *  hpc-workspace-v2
 *
 *  user.cpp
 * 
 *  - some helpers to deal with user information
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
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "user.h"

#include "fmt/base.h"
#include "fmt/ranges.h"

extern bool traceflag;
extern bool debugflag;

// get current username
std::string getUsername() {
    auto pw = getpwuid(getuid());
    std::string str(pw->pw_name);
    return str;
}

// get home of current user, we have this to avoid $HOME
std::string getUserhome()
{
    struct passwd *pw;

    pw = getpwuid(getuid());
    return std::string(pw->pw_dir);
}


// see if we are root
bool isRoot() {
    // uid is uid of caller, not 0 for setuid(root)
    if (getuid()==0) return true;
    else return false;
}


// check if this is process is not setuid
bool notSetuid() {
        // FIXME: how to check for capablity??
        return getuid() == geteuid();
}


// get list of group names of current process  // FIXME: this is same as utils::getgroupnames
std::vector<std::string> getGrouplist() {
    if(traceflag) fmt::print(stderr, "Trace  : getGroupList()\n");
    std::vector<std::string> grplist;

    // find first size and get list
    auto size = getgroups(0, nullptr);
    if (size == -1) {
        fmt::print(stderr, "Error  : error in getgroups()!\n");
        return grplist;
    }
    auto gids = new gid_t[size];
    auto ret = getgroups(size, &gids[0]);

    for(int i=0; i<ret; i++) {
            auto grpentry = getgrgid(gids[i]);
            grplist.push_back(std::string(grpentry->gr_name));
    }

    delete[] gids;

    if(debugflag) fmt::print(stderr, "groups={}\n", grplist);

    return grplist;
}
