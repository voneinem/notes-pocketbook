// T011: derive_title (first non-empty line, trimmed) and make_excerpt (UTF-8 safe).
#include <string>

#include "pbnotes/title.h"
#include "test_support.h"

using pbnotes::derive_title;
using pbnotes::make_excerpt;

namespace {
// True if s is well-formed UTF-8 (no truncated sequences).
bool valid_utf8(const std::string& s) {
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = s[i];
        size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
        if (n == 0 || i + n > s.size()) return false;
        for (size_t k = 1; k < n; ++k)
            if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return false;
        i += n;
    }
    return true;
}
}  // namespace

PB_TEST(title_is_first_line) { PB_CHECK_EQ(derive_title("Hello\nbody text"), std::string("Hello")); }

PB_TEST(title_skips_leading_blank_lines) {
    PB_CHECK_EQ(derive_title("\n\n   \n\t\n# Heading\nbody"), std::string("# Heading"));
}

PB_TEST(title_of_whitespace_only_text_is_empty) {
    PB_CHECK_EQ(derive_title(""), std::string());
    PB_CHECK_EQ(derive_title("  \n\t \r\n  "), std::string());
}

PB_TEST(title_is_trimmed) {
    PB_CHECK_EQ(derive_title("   Spaced title \t\nnext"), std::string("Spaced title"));
}

PB_TEST(title_handles_crlf) {
    PB_CHECK_EQ(derive_title("Line one\r\nLine two\r\n"), std::string("Line one"));
}

PB_TEST(title_keeps_multibyte_characters_intact) {
    const std::string t = "Größe – 日本語 🙂";
    PB_CHECK_EQ(derive_title(t + "\nrest"), t);
    PB_CHECK(valid_utf8(derive_title(t + "\nrest")));
}

PB_TEST(excerpt_skips_title_and_collapses_whitespace) {
    PB_CHECK_EQ(make_excerpt("Title\nfirst   second\n\nthird\n"), std::string("first second third"));
}

PB_TEST(excerpt_of_single_line_note_is_empty) { PB_CHECK_EQ(make_excerpt("Only a title"), std::string()); }

PB_TEST(excerpt_is_limited_to_120_characters_without_breaking_utf8) {
    std::string body;
    for (int i = 0; i < 200; ++i) body += "ä";  // 2 bytes each
    const std::string e = make_excerpt("Title\n" + body);
    PB_CHECK(valid_utf8(e));
    PB_CHECK_EQ(e.size(), static_cast<size_t>(240));  // 120 characters
}

PB_TEST_MAIN()
