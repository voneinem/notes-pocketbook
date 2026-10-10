#pragma once
// Abstract HTTP seam (T015). The production implementation (CurlTransport) verifies TLS and
// refuses non-https URLs; tests use a scripted FakeTransport. No Qt.

#include <string>
#include <utility>
#include <vector>

#include "pbnotes/types.h"

namespace pbnotes {

using Headers = std::vector<std::pair<std::string, std::string>>;

// Case-insensitive lookup; returns "" when absent.
std::string header_value(const Headers& headers, const std::string& name);
bool has_header(const Headers& headers, const std::string& name);

struct Request {
    std::string method = "GET";
    std::string url;
    Headers headers;
    std::string body;
    // HTTP Basic credentials; the transport builds the Authorization header. Empty = none.
    std::string username;
    std::string password;
    long connect_timeout_s = 10;
    long total_timeout_s = 60;
};

struct Response {
    long status = 0;  // 0 = no HTTP response (see error)
    Headers headers;
    std::string body;
    ErrorClass error = ErrorClass::None;  // transport-level classification when status == 0
    std::string error_detail;             // short, never contains credentials or note text
};

class HttpTransport {
public:
    virtual ~HttpTransport() = default;
    virtual Response perform(const Request& request) = 0;
};

}  // namespace pbnotes
