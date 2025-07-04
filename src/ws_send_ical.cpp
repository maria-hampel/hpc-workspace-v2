/*
 *  hpc-workspace-v2
 *
 *  ws_send_ical
 *
 *  - 
 *
 *  c++ version of workspace utility
 *  a workspace is a temporary directory created in behalf of a user with a limited lifetime.
 *
 *  (c) Holger Berger 2021,2023,2024,2025
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


#include "fmt/base.h"
#include "fmt/ranges.h" // IWYU pragma: keep
#include "vmime/vmime"
#include <iostream>
#include <regex>
#include <string>

#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "UserConfig.h"
#include "build_info.h"
#include "caps.h"
#include "config.h"
#include "user.h"
#include "utils.h"
#include "ws.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;
using namespace std;

// global variablesq
bool debugflag = false;
bool traceflag = false;

std::string CRLF="\r\n";

// init caps here, when euid!=uid
Cap caps{};


void commandline(po::variables_map& opt, string& filesystem, string& mailaddress, string& name, std::string& userconf, int argc, char** argv) {

    // define all options

    po::options_description cmd_options("\nOptions");
    cmd_options.add_options()("help,h", "produce help message")
        ("filesystem,F", po::value<string>(&filesystem), "filesystem where the workspace is located in")
        ("mail,m", po::value<string>(&mailaddress), "your mail address to send to")
        ("workspace,n", po::value<string>(&name), "name of selected workspace");

    po::options_description secret_options("Secret");
    secret_options.add_options()("debug", "show debugging information")("trace", "show calling information");

    // define options without names
    po::positional_options_description p;
    p.add("workspace", 1);
    p.add("mail", 2);

    po::options_description all_options;
    all_options.add(cmd_options).add(secret_options);

    //parse commandline 
    try {
        po::store(po::command_line_parser(argc, argv).options(all_options).positional(p).run(), opt);
        po::notify(opt);
    } catch (...) {
        cerr << "Usage:" << argv[0] << ": [-F filesystem | --filesystem filesystem] [-n|--workspace] workspacename [-m | --mail] mailadress" << endl;
        cerr << cmd_options << "\n";
        exit(1);
    }

    UserConfig userconfig(userconf);

    if (opt.count("help")) {
        cerr << "Usage:" << argv[0] << ": [-F filesystem | --filesystem filesystem] [-n|--workspace] workspacename [-m | --mail] mailadress" << endl;
        cerr << cmd_options << "\n";
        cerr << "this command is used to send a calendar invitation by Email to ensure users do not forget\n the expiration date of a workspace" << endl;
        exit(0);
    }

    if (opt.count("version")) {
#ifdef IS_GIT_REPOSITORY
        cout << "workspace build from git commit hash " << GIT_COMMIT_HASH << " on top of release " << WS_VERSION
             << endl;
#else
        cout << "workspace version " << WS_VERSION << endl;
#endif
        exit(0);
    }

    // globalflags
    debugflag = opt.count("debug");
    traceflag = opt.count("trace");

    /**
     * Current behaviour:
     * - workspace gets evauated first if workspacename is correct
     * - then mailadress gets evaluated, if incorrect and userconig is present just use that one
     * - other: error
     */
    static const regex e("^[[:alnum:]][[:alnum:]_.-]*$");
    if (opt.count("workspace") && regex_match(name, e)) {
        if (!opt.count("mail")) {
            mailaddress=userconfig.getMailaddress();

            if(mailaddress.length() > 0 && utils::isValidEmail(mailaddress)) {
                fmt::println(stderr,"Info   : Took email address <{}> from users config.", mailaddress);
            } else {
                fmt::println(stderr, "Error  : You can't use the ws_send_ical without a mailadress (-m).");
                exit(1);
            }
        } else {
            if (!utils::isValidEmail(mailaddress)) {
               fmt::println("Error  : Invalid email address, abort");
               exit(1);
            }
        }
    } else {
        cerr << "Error  : Illegal workspace name, use ASCII characters and numbers, '-','.' and '_' only!" << endl;
        cerr << "Usage:" << argv[0] << ": [-F filesystem | --filesystem filesystem] [-n|--workspace] workspacename [-m | --mail] mailadress" << endl;
        cerr << cmd_options << "\n";
        cerr << "this command is used to send a calendar invitation by Email to ensure users do not forget\n the expiration date of a workspace" << endl;
        exit(1);
    }

}

std::string generateDateFormat(const time_t time) {
    char timeString[std::size("yyyymmddThhmmssZ")];
    std::strftime(std::data(timeString), std::size(timeString),
                  "%Y%m%dT%H%M00Z", std::gmtime(&time));
    std::string s(timeString);
    return s;
}

std::string generateICS (const std::unique_ptr<DBEntry>& entry, time_t createtime) {
    
    /**
     * TODO:
     * - get time figured out 
     * - do you want to pass the time to the function or want to get it inside the function. from entry?
     *      - createtime is he the the ics gets created not the ws 
     */
    
    std::string wsname = entry->getId();
    time_t expirationtime = entry->getExpiration();
    std::string resource = entry->getFilesystem();
    time_t starttime = expirationtime - 7200;

    std::string starttimestr = generateDateFormat(starttime);
    std::string expirationtimestr = generateDateFormat(expirationtime);
    std::string createtimestr = generateDateFormat(createtime);
    
    
    std::size_t entryhash = std::hash<std::string>{}(entry->getId());

    std::stringstream ics;


    // HEADER
    ics << "BEGIN:VCALENDAR" << CRLF;
    ics << "VERSION:2.0" << CRLF;
    ics << "PRODID:-//HLRS Cluster Team//Workspace V2.1//EN" << CRLF; // ???
    ics << "METHOD:REQUEST" << CRLF;
    ics << "BEGIN:VTIMEZONE" << CRLF;
    ics << "TZID:Europe/Berlin" << CRLF;
    ics << "BEGIN:DAYLIGHT" << CRLF;
    ics << "TZOFFSETFROM:+0100" << CRLF;
    ics << "TZOFFSETTO:+0200" << CRLF;
    ics << "TZNAME:CEST" << CRLF;
    ics << "DTSTART:19700329T020000" << CRLF;
    ics << "RRULE:FREQ=YEARLY;BYDAY=-1SU;BYMONTH=3" << CRLF;
    ics << "END:DAYLIGHT" << CRLF;
    ics << "BEGIN:STANDARD" << CRLF;
    ics << "TZOFFSETFROM:+0200" << CRLF;
    ics << "TZOFFSETTO:+0100" << CRLF;
    ics << "TZNAME:CET" << CRLF;
    ics << "DTSTART:19701025T030000" << CRLF;
    ics << "RRULE:FREW=YEARLY;BYDAY=-1SU;BYMONTH=10" << CRLF;
    ics << "END:STANDARD" << CRLF;
    ics << "END:VTIMEZONE" << CRLF;
    ics << "X-MS-OLK-FORCEINSPECTOROPEN:TRUE" << CRLF;

    // EVENT
    ics << "BEGIN:VEVENT\r\n";
    ics << "CREATED:" << createtimestr << CRLF;
    ics << "DTSTAMP:" << createtimestr << CRLF;
    ics << "UID:587a1aa6-" << entryhash << CRLF;
    ics << "DESCRIPTION:Workspace " << wsname << " will be deleted on host " << resource << CRLF;
    ics << "LOCATION:" << resource << CRLF;
    ics << "SUMMARY:Workspace " << wsname << " expires" << CRLF;
    ics << "DTSTART;TZID=Europe/Berlin:" << starttimestr << CRLF;
    ics << "DTEND;TZID=Europe/Berlin:" << expirationtimestr << CRLF;
    ics << "LAST_MODIFIED:" << createtimestr << CRLF;
    ics << "CLASS:PRIVATE" << CRLF;
    ics << "X-MICROSOFT-CDO-BUSYSTATUS:BUSY" << CRLF;
    ics << "X-MICROSOFT-DISALLOW-COUNTER:TRUE" << CRLF;
    ics << "END:VEVENT" << CRLF;


    ics << "END:VCALENDAR" << CRLF;

    return ics.str();
}

std::string generateMail(std::unique_ptr<DBEntry>& entry){
    auto ws_name = entry->getId();
    auto resource = entry->getFilesystem();
    std::string eml_body = fmt::format("Workspace {} on host {} is going to expire ",ws_name, resource);

    return eml_body;
}

/**
 * TODO
 * -generate Mail
 * - send ical
 */


int main(int argc, char** argv) {
    string filesystem;
    string mailaddress("");
    string name;
    string configfile;
    string username;
    po::variables_map opt;
    std::string userconf;

    time_t now = time(nullptr);



    string user_conf_filename = user::getUserhome() + "/.ws_user.conf";
    if (!cppfs::is_symlink(user_conf_filename)) {
        if (cppfs::is_regular_file(user_conf_filename)) {
            userconf = utils::getFileContents(user_conf_filename.c_str());
        }
        // FIXME: could be parsed here and passed as object not string
    } else {
        fmt::print(stderr, "Error  : ~/.ws_user.conf can not be symlink!");
        exit(-1);
    }

    commandline(opt, filesystem, mailaddress, name, userconf, argc, argv);

    auto configfilestoread = std::vector<cppfs::path>{"/etc/ws.d", "/etc/ws.conf"};
    if (configfile != "") {
        if (user::isRoot() || caps.isUserMode()) {
            configfilestoread = {configfile};
        } else {
            fmt::print(stderr, "Warning: ignored config file option!\n");
        }
    }

    auto config = Config(configfilestoread);
    if (!config.isValid()) {
        fmt::println(stderr, "Error  : No valid config file found!");
        exit(-2);
    }


    // root and admins can choose usernames
    string userpattern; // used for pattern matching in DB
    if (user::isRoot() || config.isAdmin(user::getUsername())) {
        if (username != "") {
            userpattern = username;
        } else {
            userpattern = "*";
        }
    } else {
        userpattern = user::getUsername();
    }

    auto grouplist = user::getGrouplist();
    bool listgroups = false; //erstmal keine benachrichtigungen für groupworkspaces zulassen 

    vector<string> fslist;
    vector<string> validfs = config.validFilesystems(username, grouplist, ws::USE);
    if (filesystem != "") {
        if (canFind(validfs, filesystem)) {
            fslist.push_back(filesystem);
        } else {
            fmt::println(stderr, "Error  : invalid filesystem given.");
            exit(-3);
        }
    } else {
        fslist = validfs;
    }

    vector<std::unique_ptr<DBEntry>> entrylist;
    /**
     * PROBLEMO
     * - hier ist eine exception ganz wichtig: es kann 2 workspaces mit gleichem namen in 
     * aber in unterschiedlichen Filesystemen geben :((
     * - heißt wir müssen nachschauen, fall ja: Error und -F flag suggesten 
     */
    // iterate over filesystems and print or create list to be sorted
    for (auto const& fs : fslist) {
        if (debugflag)
            fmt::print("Debug  : loop over fslist {} in {}\n", fs, fslist);
        std::unique_ptr<Database> db(config.openDB(fs));

        // catch DB access errors, if DB directory or DB is accessible
        try {
            for (auto const& id : db->matchPattern(name, userpattern, grouplist, false, listgroups)) {
                std::unique_ptr<DBEntry> entry(db->readEntry(id, false));
                if (entry) {
                    entrylist.push_back(std::move(entry)); // maybe incorrect use of std::unique_ptr
                }
            }
        } catch (DatabaseException& e) {
            fmt::println(stderr, "{}", e.what());
            exit(-2);
        }
    } // loop fslist

    // multiple entries exist
    if (entrylist.size() > 1) {
        fmt::println(stderr, "Error  : multiple workspaces found, use the -F option to specify Filesystem");
        for (const std::unique_ptr<DBEntry>& entry : entrylist) {
            fmt::println(stderr, "Filesystem {}, Path {}", entry->getFilesystem(), entry->getWSPath());
        }
        exit(0);
    } else if (entrylist.empty()) {
        fmt::println(stderr, "Error  : no workspace found");
        exit(0);
    } else {
        const auto& entry=entrylist.front();
        std::string ics = generateICS(entry, now);
        // fmt::print(ics);
        fmt::print("success; filesystem {}, workspacename {}, mailaddress {}", filesystem, name, mailaddress);
    }

}