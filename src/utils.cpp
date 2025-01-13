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
#include <cassert>
namespace fs = std::filesystem;

#include "fmt/base.h"

#include <pwd.h>
#include <unistd.h>
#include <grp.h>
#include <sys/types.h>

#include "utils.h"
#include "db.h"
#include "caps.h"

using namespace std;

// globals
extern bool debugflag;
extern bool traceflag;
extern Cap caps;

namespace utils {

	// fwd
	static bool glob_match(char const *pat, char const *str);

	// read a (small) file into a string
	std::string getFileContents(const char *filename)
	{
		std::ifstream in(filename, std::ios::in | std::ios::binary);
		if (!in) {
			throw DatabaseException(fmt::format("Error  : could not open file {}", filename));
		}
		std::ostringstream contents;
		contents << in.rdbuf();
		return contents.str();
	}

	// get file names matching glob pattern from path, ("/etc", "p*d") -> passwd
	std::vector<string> dirEntries(const string path, const string pattern) {
		if (traceflag) fmt::print("Trace  : dirEntries({},{})\n", path, pattern);
		vector<string> fl;
		if (!fs::is_directory(path)) {
			fmt::println("Error   : Directory {} does not exist.", path);
			return fl;
		}
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
		if (traceflag) fmt::print("Trace  : glob_match({},{})\n", pat, str);

		/*
		* Backtrack to previous * on mismatch and retry starting one
		* character later in the string.  Because * matches all characters
		* (no exception for /), it can be easily proved that there's
		* never a need to backtrack multiple levels.
		*/
		char const *back_pat = NULL, *back_str = NULL;

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
				if (c == '\0')	/* No possible match */
					return false;
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
				[[fallthrough]];
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

	// we only support C locale, if the used local is not installed on the system
	// ws_allocate fails, this should be called in all tools
	//  problem showed with a remote SuSE machine with DE locale, coming through ssh
	void setCLocal() {
		setenv("LANG","C",1);
		setenv("LC_CTYPE","C",1);
		setenv("LC_ALL","C",1);
		std::setlocale(LC_ALL, "C");
		std::locale::global(std::locale("C"));
		// boost::filesystem::path::imbue(std::locale()); // FIXME: what is this in cpp filesystem?
	}

	// validate email address
	//  TODO: unitest?
	bool isValidEmail(const std::string& email) {
		if (traceflag) fmt::println(stderr, "Trace  : isValidEmail({})", email);
		// Regular expression for basic email validation (improved)
		const std::regex email_regex(
			R"((?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|"(?:[\x01-\x08\x0b\x0c\x0e-\x1f\x21\x23-\x5b\x5d-\x7f]|\\[\x01-\x09\x0b\x0c\x0e-\x7f])*")@(?:(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?|\[(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\]))",
			std::regex_constants::icase); // Case-insensitive

		if (!std::regex_match(email, email_regex)) {
			return false;
		}

		// Additional checks (length limits, etc.)
		if (email.length() > 254) { // RFC 5321 maximum length
			return false;
		}

		//Check for consecutive dots or @ at the beginning/end
		if (email.find("..") != std::string::npos || email.front() == '@' || email.back() == '@' || email.find(".@") != std::string::npos || email.find("@.") != std::string::npos)
		{
			return false;
		}

		return true;
	}

	// get first line of a multiline string
	std::string getFirstLine(const std::string& multilineString) {
		size_t pos = multilineString.find('\n'); 
		if (pos == std::string::npos) { 
			// No newline found, return the entire string
			return multilineString;
		} else {
			// Extract the first line
			return multilineString.substr(0, pos); 
		}
	}

    // getID returns id part of workspace id <username-id>
    std::string getID(const std::string wsid) {
        auto spos = wsid.find("-", 0);
        assert(spos!=std::string::npos);
        return wsid.substr(spos+1, wsid.length());
    }

}
