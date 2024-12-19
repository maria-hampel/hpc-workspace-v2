#include "UserConfig.h"

#ifdef WS_RAPIDYAML_CONFIG
    #define RYML_USE_ASSERT 0
    #include "ryml.hpp"
    #include "ryml_std.hpp"
    #include "c4/format.hpp"
    #include "c4/std/std.hpp"
#else
    #include "yaml-cpp/yaml.h"
#endif

#include "utils.h"


#ifdef WS_RAPIDYAML_CONFIG

// TODO: FIXME: check if used correctly in the codebase!
// read user config from string (has to be read before dropping privileges)
UserConfig::UserConfig(std::string userconf) {
    // get first line, this is either a mailaddress or something like key: value
    //  this is for compatibility with very old tools, which did not have a yaml file here
    // check if file looks like yaml
    if (userconf.find(":",0) != string::npos) {

        ryml::Tree config = ryml::parse_in_place(ryml::to_substr(userconf));  // FIXME: error check?
        ryml::NodeRef node;
        ryml::NodeRef root = config.rootref();

        // TODO: type checks if nodes are of right type?

        readRyamlScalar(config, "mail", mailaddress);

        //if (node=config["mailaddress"]; node.has_val()) node>>mailaddress; else mailaddress="";
        if (node=config["groupname"]; node.has_val()) node>>groupname; else groupname="";
        if (root.has_child("duration")) { node=config["duration"]; node>>duration; } else duration=-1;
        if (node=config["reminder"]; node.has_val()) node>>reminder; else reminder=-1;

    } else {
        // get first line of userconf only that will include the mailaddress for reminder mails
        mailaddress = utils::getFirstLine(userconf);
    }

    if (!utils::isValidEmail(mailaddress)) {
        fmt::println(stderr, "Error  : invalid email address in ~/.ws_user.conf, ignored.");
        mailaddress="";
    }
}

#else

// TODO: FIXME: check if used correctly in the codebase!
// read user config from string (has to be read before dropping privileges)
UserConfig::UserConfig(std::string userconf) {
    YAML::Node user_home_config;  // load yaml file from home here, not used anywhere else so far
    // get first line, this is either a mailaddress or something like key: value
    // std::getline(userconf, mailaddress);
    // check if file looks like yaml
    if (userconf.find(":",0) != std::string::npos) {
        user_home_config = YAML::Load(userconf);
        if (user_home_config["mail"])
            mailaddress = user_home_config["mail"].as<std::string>();
        if (user_home_config["groupname"])
            groupname = user_home_config["groupname"].as<std::string>();
        if (user_home_config["duration"])
            duration = user_home_config["duration"].as<int>();
        else
            duration = -1;
        if (user_home_config["reminder"])
            reminder = user_home_config["reminder"].as<int>();
        else
            reminder = -1;
    } else {
        // get first line of userconf only that will include the mailaddress for reminder mails
        mailaddress = utils::getFirstLine(userconf);
    }

    if (!utils::isValidEmail(mailaddress)) {
        fmt::println(stderr, "Error  : invalid email address in ~/.ws_user.conf, ignored.");
        mailaddress="";
    }

}
#endif
