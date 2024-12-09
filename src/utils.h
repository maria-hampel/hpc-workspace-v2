#ifndef UTILS_H
#define UTILS_H

/*
 *  hpc-workspace-v2
 *
 *  utils.h
 *
 *  - helper functions
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

std::vector<std::string> getgroupnames(std::string username);
std::string getFileContents(const char *filename);
inline std::string getFileContents(const std::string filename) { return getFileContents(filename.c_str()); }
std::vector<std::string> dirEntries(const std::string path, const std::string pattern);

#endif