// US1 (spec 002 FR-001, FR-002): which link targets may be followed at all, and when the text of a link
// differs from the address it really goes to (so the user is asked before the browser opens).
#include <string>

#include "pbnotes/links.h"
#include "test_support.h"

using pbnotes::is_followable_url;
using pbnotes::link_needs_confirmation;

PB_TEST(http_and_https_are_followable) {
    PB_CHECK(is_followable_url("http://example.org"));
    PB_CHECK(is_followable_url("https://example.org"));
    PB_CHECK(is_followable_url("https://example.org/path/page?q=1&r=2#frag"));
    PB_CHECK(is_followable_url("https://user@example.org:8443/x"));
    PB_CHECK(is_followable_url("https://192.168.0.1/"));
}

PB_TEST(scheme_letter_case_does_not_matter) {
    PB_CHECK(is_followable_url("HTTP://example.org"));
    PB_CHECK(is_followable_url("HtTpS://example.org/x"));
}

PB_TEST(other_schemes_are_not_followable) {
    PB_CHECK(!is_followable_url("ftp://example.org/file"));
    PB_CHECK(!is_followable_url("mailto:someone@example.org"));
    PB_CHECK(!is_followable_url("javascript:alert(1)"));
    PB_CHECK(!is_followable_url("file:///etc/passwd"));
    PB_CHECK(!is_followable_url("data:text/html;base64,AAAA"));
    PB_CHECK(!is_followable_url("tel:+491234"));
    PB_CHECK(!is_followable_url("httpx://example.org"));
}

PB_TEST(relative_and_malformed_targets_are_not_followable) {
    PB_CHECK(!is_followable_url(""));
    PB_CHECK(!is_followable_url("example.org"));
    PB_CHECK(!is_followable_url("/path/only"));
    PB_CHECK(!is_followable_url("../up.html"));
    PB_CHECK(!is_followable_url("#anchor"));
    PB_CHECK(!is_followable_url("http:/example.org"));
    PB_CHECK(!is_followable_url("http//example.org"));
}

PB_TEST(an_empty_host_is_not_followable) {
    PB_CHECK(!is_followable_url("http://"));
    PB_CHECK(!is_followable_url("https:///path"));
    PB_CHECK(!is_followable_url("https://?q=1"));
    PB_CHECK(!is_followable_url("https://#frag"));
    PB_CHECK(!is_followable_url("https://user@/path"));
    PB_CHECK(!is_followable_url("https://:8080/"));
}

PB_TEST(whitespace_and_control_characters_are_not_followable) {
    PB_CHECK(!is_followable_url("https://example.org/a b"));
    PB_CHECK(!is_followable_url("https://example.org/a\tb"));
    PB_CHECK(!is_followable_url("https://example.org/a\nb"));
    PB_CHECK(!is_followable_url(" https://example.org"));
    PB_CHECK(!is_followable_url("https://example.org "));
    PB_CHECK(!is_followable_url(std::string("https://example.org/a") + '\x01'));
    PB_CHECK(!is_followable_url(std::string("https://example.org/a") + '\x7f'));
    PB_CHECK(!is_followable_url(std::string("https://example.org/a") + '\0' + "b"));
}

PB_TEST(the_length_limit_is_2048_bytes) {
    const std::string base = "https://example.org/";
    PB_CHECK(is_followable_url(base + std::string(2048 - base.size(), 'a')));
    PB_CHECK(!is_followable_url(base + std::string(2049 - base.size(), 'a')));
}

PB_TEST(a_link_text_that_is_the_address_needs_no_confirmation) {
    PB_CHECK(!link_needs_confirmation("https://example.org", "https://example.org"));
    PB_CHECK(!link_needs_confirmation("https://example.org/a/b?c=1", "https://example.org/a/b?c=1"));
}

PB_TEST(scheme_www_host_case_and_one_trailing_slash_are_ignored) {
    PB_CHECK(!link_needs_confirmation("example.org", "https://example.org"));
    PB_CHECK(!link_needs_confirmation("http://example.org", "https://example.org"));
    PB_CHECK(!link_needs_confirmation("www.example.org", "https://example.org"));
    PB_CHECK(!link_needs_confirmation("https://www.example.org", "https://example.org"));
    PB_CHECK(!link_needs_confirmation("EXAMPLE.org", "https://example.ORG"));
    PB_CHECK(!link_needs_confirmation("HTTPS://Example.Org/", "https://example.org"));
    PB_CHECK(!link_needs_confirmation("https://example.org", "https://example.org/"));
    PB_CHECK(!link_needs_confirmation("example.org/page/", "https://www.example.org/page"));
}

PB_TEST(a_path_is_compared_exactly) {
    PB_CHECK(link_needs_confirmation("https://example.org/A", "https://example.org/a"));
    PB_CHECK(link_needs_confirmation("https://example.org//", "https://example.org"));
}

PB_TEST(descriptive_text_needs_confirmation) {
    PB_CHECK(link_needs_confirmation("Read this", "https://example.org"));
    PB_CHECK(link_needs_confirmation("", "https://example.org"));
    PB_CHECK(link_needs_confirmation("click here", "https://example.org/a"));
}

PB_TEST(a_different_destination_needs_confirmation) {
    PB_CHECK(link_needs_confirmation("https://example.org", "https://evil.example.net"));
    PB_CHECK(link_needs_confirmation("https://example.org/a", "https://example.org/b"));
    PB_CHECK(link_needs_confirmation("https://example.org/a?x=1", "https://example.org/a?x=2"));
    PB_CHECK(link_needs_confirmation("https://example.org", "https://example.org:8443"));
    PB_CHECK(link_needs_confirmation("example.org", "https://example.org.evil.net"));
}

PB_TEST(text_that_merely_contains_the_address_needs_confirmation) {
    PB_CHECK(link_needs_confirmation("see https://example.org", "https://example.org"));
    PB_CHECK(link_needs_confirmation("https://example.org now", "https://example.org"));
    PB_CHECK(link_needs_confirmation("https://example.org https://example.org", "https://example.org"));
}

PB_TEST_MAIN()
