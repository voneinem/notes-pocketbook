#include "pbnotes/log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <mutex>

namespace pbnotes {
namespace log {

namespace {

std::mutex g_mutex;
std::function<void(const std::string&)> g_sink;

bool is_sensitive_name(const std::string& name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return std::tolower(c); });
    static const char* const kDenied[] = {"pass", "secret", "token", "auth", "user", "content", "title",
                                          "body", "note", "url", "mail", "cookie", "key", "name"};
    for (const char* d : kDenied)
        if (n.find(d) != std::string::npos) return true;
    return false;
}

std::string sanitize(std::string v) {
    for (char& c : v)
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    return v;
}

std::string timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

}  // namespace

Field status(long http_status) { return {"status", std::to_string(http_status), true}; }
Field count(const std::string& name, long value) { return {name, std::to_string(value), true}; }
Field path(const std::string& file_path) { return {"path", file_path, true}; }
Field text(const std::string& name, const std::string& value) { return {name, value, false}; }

void event(const std::string& code, const std::vector<Field>& fields) {
    std::string line = timestamp() + " " + sanitize(code);
    for (const Field& f : fields) {
        line += " " + sanitize(f.name) + "=";
        line += (!f.typed && is_sensitive_name(f.name)) ? "[redacted]" : sanitize(f.value);
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_sink) g_sink(line);
    else std::fprintf(stderr, "%s\n", line.c_str());
}

void event(const std::string& code, std::initializer_list<Field> fields) {
    event(code, std::vector<Field>(fields));
}

void set_sink(std::function<void(const std::string&)> sink) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_sink = std::move(sink);
}

void reset_sink() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_sink = nullptr;
}

}  // namespace log
}  // namespace pbnotes
