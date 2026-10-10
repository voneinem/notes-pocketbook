#include "pbnotes/http_transport.h"

#include <algorithm>
#include <cctype>

namespace pbnotes {

namespace {
bool iequals(const std::string& a, const std::string& b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](unsigned char x, unsigned char y) {
               return std::tolower(x) == std::tolower(y);
           });
}
}  // namespace

std::string header_value(const Headers& headers, const std::string& name) {
    for (const auto& h : headers)
        if (iequals(h.first, name)) return h.second;
    return std::string();
}

bool has_header(const Headers& headers, const std::string& name) {
    for (const auto& h : headers)
        if (iequals(h.first, name)) return true;
    return false;
}

}  // namespace pbnotes
