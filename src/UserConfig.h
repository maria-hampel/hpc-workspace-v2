#pragma once

#include <string>

// config in user home
class UserConfig {
private:
    std::string mailaddress;        // default mailaddress for reminder
    std::string groupname;          // FIXME: TODO: does this make sense?
    int reminder;                   // days before expiration to send reminder email
    int duration;                   // default diration

public:
    // read config from string, either YAML or single line
    UserConfig(std::string userconf);
    // get mailaddress
    std::string getMailaddress() const { return mailaddress; };
    std::string getGroupname() const { return groupname; };
    int getReminder() const { return reminder; };
    int getDuration() const { return duration; };
};
