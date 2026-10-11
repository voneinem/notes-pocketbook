// Spec 004 (FR-004, FR-005): title rules. Trimmed of leading/trailing ASCII whitespace; non-empty after trimming;
// at most 255 code points; valid UTF-8; no control characters (U+0000-U+001F, U+007F).
#include <string>

#include "pbnotes/note_store.h"
#include "test_support.h"

using pbnotes::trim_title;
using pbnotes::validate_title;

namespace {
std::string repeat(const std::string& unit, int n) {
    std::string out;
    for (int i = 0; i < n; ++i) out += unit;
    return out;
}
}  // namespace

PB_TEST(empty_and_whitespace_only_titles_are_empty) {
    PB_CHECK_EQ(validate_title(""), std::string("title_empty"));
    PB_CHECK_EQ(validate_title("   "), std::string("title_empty"));
    PB_CHECK_EQ(validate_title("\t\r\n"), std::string("title_empty"));
}

PB_TEST(a_title_is_trimmed_before_it_is_judged) {
    PB_CHECK_EQ(validate_title("Groceries"), std::string());
    PB_CHECK_EQ(validate_title("  Groceries  "), std::string());
    PB_CHECK_EQ(trim_title("  Groceries  "), std::string("Groceries"));
    PB_CHECK_EQ(trim_title("\tGroceries\r\n"), std::string("Groceries"));
    PB_CHECK_EQ(validate_title("Groceries\n"), std::string());  // a trailing line break is trimmed, not rejected
    PB_CHECK_EQ(trim_title("a b"), std::string("a b"));
}

PB_TEST(the_limit_is_255_code_points_not_bytes) {
    PB_CHECK_EQ(validate_title(repeat("a", 255)), std::string());
    PB_CHECK_EQ(validate_title(repeat("a", 256)), std::string("title_too_long"));
    PB_CHECK_EQ(validate_title(repeat("\xC3\xA4", 255)), std::string());                 // 2-byte letters
    PB_CHECK_EQ(validate_title(repeat("\xC3\xA4", 256)), std::string("title_too_long"));
    PB_CHECK_EQ(validate_title(repeat("\xF0\x9F\x98\x80", 255)), std::string());         // 255 emoji
    PB_CHECK_EQ(validate_title(repeat("\xF0\x9F\x98\x80", 256)), std::string("title_too_long"));
}

PB_TEST(a_title_is_measured_after_trimming) {
    PB_CHECK_EQ(validate_title("  " + repeat("a", 255) + "  "), std::string());
}

PB_TEST(control_characters_are_refused) {
    PB_CHECK_EQ(validate_title("a\nb"), std::string("title_control_chars"));
    PB_CHECK_EQ(validate_title("a\tb"), std::string("title_control_chars"));
    PB_CHECK_EQ(validate_title(std::string("a\x01" "b")), std::string("title_control_chars"));
    PB_CHECK_EQ(validate_title(std::string("a\x7f")), std::string("title_control_chars"));
    PB_CHECK_EQ(validate_title(std::string("a\0b", 3)), std::string("title_control_chars"));
}

PB_TEST(invalid_utf8_is_refused) {
    PB_CHECK_EQ(validate_title("\xC3"), std::string("title_invalid"));          // truncated
    PB_CHECK_EQ(validate_title("a\xC3("), std::string("title_invalid"));        // bad continuation
    PB_CHECK_EQ(validate_title("\xC0\xAF"), std::string("title_invalid"));      // overlong
    PB_CHECK_EQ(validate_title("\xED\xA0\x80"), std::string("title_invalid"));  // surrogate
    PB_CHECK_EQ(validate_title("\xF5\x80\x80\x80"), std::string("title_invalid"));  // beyond U+10FFFF
}

PB_TEST(characters_a_server_may_restrict_are_accepted_here) {
    PB_CHECK_EQ(validate_title("a/b \\ c: d* e? \"f\" <g> |h"), std::string());
    PB_CHECK_EQ(validate_title("Größe 日本語 \xF0\x9F\x98\x80"), std::string());
}

PB_TEST(when_several_rules_fail_invalid_utf8_comes_first_then_control_then_length) {
    PB_CHECK_EQ(validate_title(std::string("\xC3\n") + repeat("a", 300)), std::string("title_invalid"));
    PB_CHECK_EQ(validate_title("a\n" + repeat("a", 300)), std::string("title_control_chars"));
    PB_CHECK_EQ(validate_title(repeat("a", 300)), std::string("title_too_long"));
}

PB_TEST_MAIN()
