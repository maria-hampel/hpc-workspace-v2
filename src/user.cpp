#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>


#include "user.h"

// get current username
std::string getUsername() {
    auto pw = getpwuid(getuid());
    std::string str(pw->pw_name);
    return str;
}

// see if we are root
bool isRoot() {
    // uid is uid of caller, not 0 for setuid(root)
    if (getuid()==0) return true;
    else return false;
}


// check if this is process is not setuid
bool notSetuid() {
        return getuid() == geteuid();
}


// get list of group names of current process
std::vector<std::string> getGrouplist() {
    std::vector<std::string> grplist;

    // find first size and get list
    auto size = getgroups(0, nullptr);
    if (size == -1) return grplist;
    auto gids = new gid_t[size];
    auto ret = getgroups(size, &gids[0]);

    for(int i=0; i<ret; i++) {
            auto grpentry = getgrgid(gids[i]);
            grplist.push_back(std::string(grpentry->gr_name));
    }

    return grplist;
}
