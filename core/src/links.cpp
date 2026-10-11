#include "pbnotes/links.h"

#include <string>

namespace pbnotes {

namespace {

constexpr size_t kMaxUrlBytes = 2048;

char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

bool starts_with_icase(std::string_view s, std::string_view prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i)
        if (lower(s[i]) != prefix[i]) return false;
    return true;
}

// Length of a leading "http://" or "https://" (any case), else 0.
size_t scheme_length(std::string_view s) {
    if (starts_with_icase(s, "https://")) return 8;
    if (starts_with_icase(s, "http://")) return 7;
    return 0;
}

bool has_blank_or_control(std::string_view s) {
    for (unsigned char c : s)
        if (c <= 0x20 || c == 0x7f) return true;
    return false;
}

// Scheme, "www." and one trailing slash removed; the host (up to the first / ? #) in lower case.
std::string normalized(std::string_view s) {
    s.remove_prefix(scheme_length(s));
    if (starts_with_icase(s, "www.")) s.remove_prefix(4);
    if (!s.empty() && s.back() == '/') s.remove_suffix(1);
    std::string out(s);
    const size_t host_end = out.find_first_of("/?#");
    for (size_t i = 0; i < (host_end == std::string::npos ? out.size() : host_end); ++i) out[i] = lower(out[i]);
    return out;
}

}  // namespace

bool is_followable_url(std::string_view url) {
    if (url.size() > kMaxUrlBytes || has_blank_or_control(url)) return false;
    const size_t scheme = scheme_length(url);
    if (scheme == 0) return false;
    std::string_view rest = url.substr(scheme);
    const size_t authority_end = rest.find_first_of("/?#");
    std::string_view authority = rest.substr(0, authority_end);
    const size_t at = authority.rfind('@');
    if (at != std::string_view::npos) authority.remove_prefix(at + 1);
    // the host is what remains before an optional :port
    size_t host_len = authority.size();
    if (!authority.empty() && authority.front() == '[') {  // IPv6 literal
        const size_t close = authority.find(']');
        host_len = close == std::string_view::npos ? 0 : close + 1;
    } else {
        const size_t colon = authority.find(':');
        if (colon != std::string_view::npos) host_len = colon;
    }
    return host_len > 0;
}

bool link_needs_confirmation(std::string_view text, std::string_view url) {
    if (text.empty() || has_blank_or_control(text)) return true;
    return normalized(text) != normalized(url);
}

}  // namespace pbnotes
