// Spec 005 FR-008 / FR-032: text arriving from the clipboard is untrusted. clean_pasted_text drops control
// characters, normalizes line breaks and refuses invalid UTF-8 or too much text; single_line makes it fit the
// one-line title.
#include <string>

#include "pbnotes/text_clean.h"
#include "test_support.h"

using pbnotes::clean_pasted_text;
using pbnotes::single_line;

namespace {
std::string repeat(const std::string& unit, size_t n) {
    std::string out;
    out.reserve(unit.size() * n);
    for (size_t i = 0; i < n; ++i) out += unit;
    return out;
}
}  // namespace

PB_TEST(control_characters_are_dropped) {
    auto r = clean_pasted_text("a\x01" "b");
    PB_CHECK(r.ok);
    PB_CHECK_EQ(r.text, std::string("ab"));
}

PB_TEST(tab_and_newline_are_kept) {
    auto r = clean_pasted_text("a\tb\nc");
    PB_CHECK(r.ok);
    PB_CHECK_EQ(r.text, std::string("a\tb\nc"));
}

PB_TEST(line_breaks_are_normalized_to_newline) {
    for (const char* in : {"a\r\nb", "a\rb", "a\xE2\x80\xA8" "b", "a\xE2\x80\xA9" "b"}) {
        auto r = clean_pasted_text(in);
        PB_CHECK(r.ok);
        PB_CHECK_EQ(r.text, std::string("a\nb"));
    }
}

PB_TEST(nul_del_and_c1_controls_are_dropped) {
    PB_CHECK_EQ(clean_pasted_text(std::string("a\0b", 3)).text, std::string("ab"));
    PB_CHECK_EQ(clean_pasted_text("a\x7f" "b").text, std::string("ab"));
    PB_CHECK_EQ(clean_pasted_text("a\xC2\x85" "b").text, std::string("ab"));   // U+0085
    PB_CHECK_EQ(clean_pasted_text("a\xC2\x9F" "b").text, std::string("ab"));   // U+009F
    PB_CHECK_EQ(clean_pasted_text("a\xC2\xA0" "b").text, std::string("a\xC2\xA0" "b"));  // U+00A0 stays
}

PB_TEST(invalid_utf8_is_refused) {
    for (const char* in : {"\xC3(", "\xFF", "\xC0\xAF", "\xED\xA0\x80" /* surrogate */, "\xF4\x90\x80\x80" /* > U+10FFFF */}) {
        auto r = clean_pasted_text(in);
        PB_CHECK(!r.ok);
        PB_CHECK_EQ(r.error_key, std::string("paste_invalid"));
    }
    // a truncated sequence at the end
    auto r = clean_pasted_text("abc\xE2\x82");
    PB_CHECK(!r.ok);
}

PB_TEST(empty_input_is_ok_and_empty) {
    auto r = clean_pasted_text("");
    PB_CHECK(r.ok);
    PB_CHECK(r.text.empty());
}

PB_TEST(the_limit_counts_code_points_not_bytes) {
    auto ok = clean_pasted_text(repeat("\xC3\xA4", 1000000));  // 2,000,000 bytes, 1,000,000 characters
    PB_CHECK(ok.ok);
    PB_CHECK_EQ(ok.text.size(), static_cast<size_t>(2000000));
    auto big = clean_pasted_text(repeat("\xC3\xA4", 1000001));
    PB_CHECK(!big.ok);
    PB_CHECK_EQ(big.error_key, std::string("paste_too_large"));
}

PB_TEST(a_smaller_limit_can_be_given) {
    PB_CHECK(clean_pasted_text("abcd", 4).ok);
    PB_CHECK_EQ(clean_pasted_text("abcde", 4).error_key, std::string("paste_too_large"));
}

PB_TEST(emoji_and_combining_marks_pass_through) {
    const std::string s = "\xF0\x9F\x98\x80 e\xCC\x81 \xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9";  // 😀, é (combining), ZWJ pair
    auto r = clean_pasted_text(s);
    PB_CHECK(r.ok);
    PB_CHECK_EQ(r.text, s);
}

PB_TEST(single_line_turns_breaks_and_tabs_into_one_space) {
    PB_CHECK_EQ(single_line("a\n\nb"), std::string("a b"));
    PB_CHECK_EQ(single_line("a\r\nb"), std::string("a b"));
    PB_CHECK_EQ(single_line("\tx"), std::string(" x"));
    PB_CHECK_EQ(single_line("a\t\tb"), std::string("a b"));
    PB_CHECK_EQ(single_line("x\n"), std::string("x "));  // not trimmed
    PB_CHECK_EQ(single_line("one line"), std::string("one line"));
    PB_CHECK_EQ(single_line("a  b"), std::string("a  b"));  // ordinary spaces are untouched
}

PB_TEST_MAIN()
