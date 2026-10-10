#pragma once
// Structured logger (constitution Principle III): logs event codes, HTTP status, counts and
// file paths only. Fields whose NAME suggests sensitive data (password, user name, content,
// title, authorization, token, url, ...) are always redacted, so personal data cannot leak
// through a careless call.

#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

namespace pbnotes {
namespace log {

struct Field {
    std::string name;
    std::string value;
    bool typed = false;  // numeric or path fields are not subject to name-based redaction
};

Field status(long http_status);                    // status=<n>
Field count(const std::string& name, long value);  // <name>=<n>
Field path(const std::string& file_path);          // path=<p>
Field text(const std::string& name, const std::string& value);  // short non-sensitive text

// Writes one line: "<time> <code> name=value ...". Newlines in values are replaced.
void event(const std::string& code, std::initializer_list<Field> fields);
void event(const std::string& code, const std::vector<Field>& fields);

// Default sink writes to stderr. Tests (and the app) can capture lines.
void set_sink(std::function<void(const std::string& line)> sink);
void reset_sink();

}  // namespace log
}  // namespace pbnotes
