#pragma once
// Setup-time connection test (T025/T030): URL validation, then a capabilities request.

#include <string>

#include "pbnotes/api_client.h"
#include "pbnotes/ca_selector.h"
#include "pbnotes/http_transport.h"

namespace pbnotes {

struct UrlCheck {
    bool ok = false;
    std::string normalized;  // https://host[:port][/path] without trailing slash
    std::string error_key;   // url_empty | url_not_https | url_invalid
};

// Only https:// URLs are accepted. Credentials, queries and fragments in the URL are rejected.
UrlCheck normalize_server_url(const std::string& input);

struct ConnectionTestResult {
    bool ok = false;
    ErrorClass error_class = ErrorClass::None;
    // ok | url_empty | url_not_https | url_invalid | credentials_missing | auth_rejected |
    // server_unreachable | certificate_unverifiable | server_unsupported | server_maintenance |
    // server_error
    std::string message_key;
    CaSource ca_source = CaSource::Bundled;  // which CA file the transport trusts (for display)
    std::string normalized_url;
    std::string detail;  // technical cause for network/TLS failures, e.g. "curl 6: Could not resolve host"
};

class ConnectionTester {
public:
    ConnectionTester(HttpTransport& transport, CaSource ca_source) : transport_(transport), ca_source_(ca_source) {}
    ConnectionTestResult test(const std::string& url, const std::string& username, const std::string& password);

private:
    HttpTransport& transport_;
    CaSource ca_source_;
};

}  // namespace pbnotes
