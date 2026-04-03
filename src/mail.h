#ifndef MAIL_H
#define MAIL_H

#include <ctime>
#include <curl/curl.h>
#include <string>
#include <vector>

#include "fmt/base.h"

namespace mail {

// Mail helper struct
struct EmailData {
    std::string content;
    size_t index;

    EmailData(const std::string& content) : content(content), index(0) {}
};

// Initialize Curl
void initCurl();

// Cleanup Curl
void cleanupCurl();

// Send a Mail with curl to the smtpUrl
bool sendCurl(const std::string& smtpUrl, const std::string& mail_from, std::vector<std::string>& mail_to,
              const std::string& completeMail);

// Generate the Date Format used for Mime Mails from time_t
std::string generateMailDateFormat(const time_t time);

// Generate a message ID from current time, PID, and Random component
// Domain parameter is required - no default
std::string generateMessageID(const std::string& domain);

// Generate the To Header for Mails
std::string generateToHeader(std::vector<std::string> mail_to);

} // namespace mail

#endif
