/*
 *  hpc-workspace-v2
 *
 *  utils.cpp
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
#include <fstream>
#include <iomanip>
#include <sstream>
#include <filesystem>
namespace fs = std::filesystem;

#include "fmt/base.h"

#include <pwd.h>
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>



#include "utils.h"

using namespace std;

extern bool debugflag;
extern bool traceflag;

namespace utils {

	// fwd
	static bool glob_match(char const *pat, char const *str);

	/* FIXME: unused, can be removed
	// get list of groupnames for a user
	std::vector<std::string> getgroupnameS(std::string username) {
		vector<string> groupnames;

		struct group *grp;
		int ngroups = 128;
		gid_t gids[128];
		int nrgroups;

		// FIXME:: does getgid return the right value here? pw->pw_gid?
		//  if current group is not primary group in /etc/passwd, will this return all groups?
		nrgroups = getgrouplist(username.c_str(), getgid(), gids, &ngroups); 
		if(nrgroups == -1) {
			fmt::print(stderr, "Error  : user in too many groups!\n");
			exit(-1); // FIXME: error return, unlikely see constant above
		}
		for(int i=0; i<nrgroups; i++) {
			grp=getgrgid(gids[i]);
			if(grp) {
				groupnames.push_back(string(grp->gr_name));
				if (debugflag) {
					fmt::print(stderr, "debug: secondary group {}\n", string(grp->gr_name));
				}
			}
		}
		// get current group
		grp=getgrgid(getgid());
		if (grp==NULL) {
			fmt::print(stderr, "Error  : user has no group anymore!");
			exit(-1); // FIXME: error return
		}
		groupnames.push_back(string(grp->gr_name)); // FIXME: is this necessary or is it already in the list?

		return groupnames;
	}
	*/


	// read a (small) file into a string
	std::string getFileContents(const char *filename)
	{
		std::ifstream in(filename, std::ios::in | std::ios::binary);
		if (!in) {
			fmt::print(stderr, "Error  : could not open {}\n", filename);
			exit(1);
		}
		std::ostringstream contents;
		contents << in.rdbuf();
		return contents.str();
	}

	// get file names matching glob pattern from path, ("/etc", "p*d") -> passwd
	std::vector<string> dirEntries(const string path, const string pattern) {
		if (traceflag) fmt::print("dirEntries({},{})\n", path, pattern);
		vector<string> fl;
		for (const auto & entry : fs::directory_iterator(path)) {
			if (entry.is_regular_file())
				if (glob_match(pattern.c_str(), entry.path().filename().string().c_str())) {
					fl.push_back(entry.path().filename().string());
				}
		}
		return fl;
	}


	// glob matching stolen from linux kernel, under MIT/GPL
	//   https://github.com/torvalds/linux/blob/master/lib/glob.c
	static bool glob_match(char const *pat, char const *str)
	{
		if (traceflag) fmt::print("glob_match({},{})\n", pat, str);
		/*
		* Backtrack to previous * on mismatch and retry starting one
		* character later in the string.  Because * matches all characters
		* (no exception for /), it can be easily proved that there's
		* never a need to backtrack multiple levels.
		*/
		char const *back_pat = NULL, *back_str = back_str;
		/*
		* Loop over each token (character or class) in pat, matching
		* it against the remaining unmatched tail of str.  Return false
		* on mismatch, or true after matching the trailing nul bytes.
		*/
		for (;;) {
			unsigned char c = *str++;
			unsigned char d = *pat++;
			switch (d) {
			case '?':	/* Wildcard: anything but nul */
				if (c == '\0')
					return false;
				break;
			case '*':	/* Any-length wildcard */
				if (*pat == '\0')	/* Optimize trailing * case */
					return true;
				back_pat = pat;
				back_str = --str;	/* Allow zero-length match */
				break;
			case '[': {	/* Character class */
				bool match = false, inverted = (*pat == '!');
				char const *cclass = pat + inverted;
				unsigned char a = *cclass++;
				/*
				* Iterate over each span in the character class.
				* A span is either a single character a, or a
				* range a-b.  The first span may begin with ']'.
				*/
				do {
					unsigned char b = a;
					if (a == '\0')	/* Malformed */
						goto literal;
					if (cclass[0] == '-' && cclass[1] != ']') {
						b = cclass[1];
						if (b == '\0')
							goto literal;
						cclass += 2;
						/* Any special action if a > b? */
					}
					match |= (a <= c && c <= b);
				} while ((a = *cclass++) != ']');
				if (match == inverted)
					goto backtrack;
				pat = cclass;
				}
				break;
			case '\\':
				d = *pat++;
				/*FALLTHROUGH*/
			default:	/* Literal character */
	literal:
				if (c == d) {
					if (d == '\0')
						return true;
					break;
				}
	backtrack:
				if (c == '\0' || !back_pat)
					return false;	/* No point continuing */
				/* Try again from last *, one character later in str. */
				pat = back_pat;
				str = ++back_str;
				break;
			}
		}
	}

}