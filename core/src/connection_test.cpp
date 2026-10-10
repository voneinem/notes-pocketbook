#include "pbnotes/connection_test.h"

#include <algorithm>
#include <cctype>

namespace pbnotes {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

UrlCheck fail(const char* key) {
    UrlCheck r;
    r.error_key = key;
    return r;
}

bool valid_host(const std::string& host) {
    if (host.empty() || host.front() == '.' || host.back() == '.' || host.front() == '-') return false;
    for (char c : host)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-')) return false;
    return true;
}

}  // namespace

UrlCheck normalize_server_url(const std::string& input) {
    const std::string url = trim(input);
    if (url.empty()) return fail("url_empty");
    for (char c : url)
        if (std::isspace(static_cast<unsigned char>(c)) || std::iscntrl(static_cast<unsigned char>(c))) return fail("url_invalid");

    const size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos || lower(url.substr(0, scheme_end)) != "https") return fail("url_not_https");

    std::string rest = url.substr(scheme_end + 3);
    if (rest.find('?') != std::string::npos || rest.find('#') != std::string::npos) return fail("url_invalid");

    const size_t slash = rest.find('/');
    std::string authority = slash == std::string::npos ? rest : rest.substr(0, slash);
    std::string path = slash == std::string::npos ? std::string() : rest.substr(slash);
    if (authority.empty() || authority.find('@') != std::string::npos) return fail("url_invalid");

    std::string host = authority;
    std::string port;
    const size_t colon = authority.rfind(':');
    if (colon != std::string::npos) {
        host = authority.substr(0, colon);
        port = authority.substr(colon + 1);
        if (port.empty() || port.size() > 5 || !std::all_of(port.begin(), port.end(), [](unsigned char c) { return std::isdigit(c); }))
            return fail("url_invalid");
        const int p = std::stoi(port);
        if (p < 1 || p > 65535) return fail("url_invalid");
    }
    host = lower(host);
    if (!valid_host(host)) return fail("url_invalid");

    while (!path.empty() && path.back() == '/') path.pop_back();

    UrlCheck ok;
    ok.ok = true;
    ok.normalized = "https://" + host + (port.empty() ? "" : ":" + port) + path;
    return ok;
}

ConnectionTestResult ConnectionTester::test(const std::string& url, const std::string& username,
                                            const std::string& password) {
    ConnectionTestResult result;
    result.ca_source = ca_source_;

    const UrlCheck check = normalize_server_url(url);
    if (!check.ok) {
        result.message_key = check.error_key;
        return result;
    }
    result.normalized_url = check.normalized;
    if (username.empty() || password.empty()) {
        result.message_key = "credentials_missing";
        return result;
    }

    ApiClient api(transport_, {check.normalized, username, password});
    const CapabilitiesResult caps = api.fetch_capabilities();
    result.error_class = caps.error;
    result.detail = caps.detail;
    switch (caps.error) {
        case ErrorClass::None:
            result.ok = true;
            result.message_key = "ok";
            break;
        case ErrorClass::Auth: result.message_key = "auth_rejected"; break;
        case ErrorClass::Network: result.message_key = "server_unreachable"; break;
        case ErrorClass::Tls: result.message_key = "certificate_unverifiable"; break;
        case ErrorClass::Unsupported: result.message_key = "server_unsupported"; break;
        case ErrorClass::Maintenance: result.message_key = "server_maintenance"; break;
        case ErrorClass::Server: result.message_key = "server_error"; break;
    }
    return result;
}

}  // namespace pbnotes
