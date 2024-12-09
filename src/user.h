#ifndef USER_H
#define USER_H

#include <string>
#include <vector>

#include <sys/types.h>
#include <pwd.h>

std::string getUsername();
std::string getUserhome();
bool isRoot();
bool notSetuid();
std::vector<std::string> getGrouplist();

#endif
