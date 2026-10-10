// T019: the logger accepts only event codes, HTTP status, counts and file paths; sensitive
// field names are redacted (constitution Principle III).
#include <string>
#include <vector>

#include "pbnotes/log.h"
#include "test_support.h"

namespace log = pbnotes::log;

namespace {
struct Capture {
    std::vector<std::string> lines;
    Capture() {
        log::set_sink([this](const std::string& line) { lines.push_back(line); });
    }
    ~Capture() { log::reset_sink(); }
    std::string all() const {
        std::string s;
        for (const auto& l : lines) s += l + "\n";
        return s;
    }
};
bool contains(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }
}  // namespace

PB_TEST(logs_event_status_counts_and_path) {
    Capture c;
    log::event("sync.finished", {log::status(200), log::count("pushed", 3), log::path("/mnt/ext1/system/pbnotes/pbnotes.db")});
    PB_CHECK_EQ(c.lines.size(), static_cast<size_t>(1));
    PB_CHECK(contains(c.all(), "sync.finished"));
    PB_CHECK(contains(c.all(), "status=200"));
    PB_CHECK(contains(c.all(), "pushed=3"));
    PB_CHECK(contains(c.all(), "path=/mnt/ext1/system/pbnotes/pbnotes.db"));
}

PB_TEST(sensitive_field_names_are_redacted) {
    Capture c;
    log::event("account.saved", {log::text("password", "hunter2"), log::text("USERNAME", "alice"),
                                 log::text("content", "my secret diary"), log::text("title", "My diary"),
                                 log::text("Authorization", "Basic YWxpY2U6aHVudGVyMg=="),
                                 log::text("token", "abc123"), log::text("url", "https://alice:pw@host/")});
    const std::string out = c.all();
    for (const char* leaked : {"hunter2", "alice", "diary", "YWxpY2U6", "abc123", "pw@host"})
        PB_CHECK(!contains(out, leaked));
    PB_CHECK(contains(out, "password=[redacted]"));
}

PB_TEST(harmless_text_fields_are_kept) {
    Capture c;
    log::event("sync.finished", {log::text("outcome", "success"), log::text("error_class", "none")});
    PB_CHECK(contains(c.all(), "outcome=success"));
}

PB_TEST(newlines_in_values_cannot_forge_log_lines) {
    Capture c;
    log::event("x", {log::text("outcome", "ok\nFAKE LINE")});
    PB_CHECK_EQ(c.lines.size(), static_cast<size_t>(1));
    PB_CHECK(!contains(c.lines[0], "\n"));
}

PB_TEST_MAIN()
