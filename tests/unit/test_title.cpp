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

namespace {
// The excerpt of "T\n" + `body` limited to `max` characters.
std::string excerpt_of(const std::string& body, size_t max) { return make_excerpt("T\n" + body, max); }
}  // namespace

PB_TEST(excerpt_never_ends_inside_an_emoji_sequence) {
    const std::string kinds[] = {
        "\U0001F600",                                                            // single
        "\u2764\uFE0F",                                                          // variation selector
        "\U0001F44D\U0001F3FD",                                                  // skin tone
        "\U0001F468\u200D\U0001F469\u200D\U0001F467",                           // ZWJ family
        "\U0001F1E9\U0001F1EA",                                                  // flag
        "1\uFE0F\u20E3",                                                         // keycap
        "\U0001F3F4\U000E0067\U000E0062\U000E0065\U000E006E\U000E0067\U000E007F",  // tag sequence
        "e\u0301",                                                               // combining mark
    };
    for (const std::string& unit : kinds) {
        // the unit straddles the limit for every cut position inside it
        for (size_t before = 0; before < 4; ++before) {
            const std::string body = std::string(before, 'a') + unit + " tail text";
            for (size_t max = 1; max < before + 8; ++max) {
                const std::string e = excerpt_of(body, max);
                PB_CHECK(valid_utf8(e));
                // if the excerpt contains the first byte of the unit it must contain the whole unit
                const size_t at = e.find(unit.substr(0, 1) == "1" ? "1" : unit.substr(0, 4 > unit.size() ? unit.size() : 3));
                if (at != std::string::npos && at >= before && e.size() > before) {
                    PB_CHECK(e.compare(before, unit.size(), unit) == 0);
                }
            }
        }
    }
}

PB_TEST(excerpt_may_exceed_the_limit_by_one_unit) {
    const std::string family = "\U0001F468\u200D\U0001F469\u200D\U0001F467";
    // limit 3: "ab" + the family (5 code points) -> the family is kept whole
    const std::string e = excerpt_of("ab" + family + "xyz", 3);
    PB_CHECK_EQ(e, "ab" + family);
    // the next unit is not started once the limit is reached
    PB_CHECK_EQ(excerpt_of("abc" + family, 3), std::string("abc"));
}

PB_TEST(excerpt_of_text_without_emojis_is_unchanged) {
    PB_CHECK_EQ(excerpt_of("abcdef", 3), std::string("abc"));
    PB_CHECK_EQ(excerpt_of("a b  c", 4), std::string("a b "));  // collapsed spaces count as before
    std::string body;
    for (int i = 0; i < 200; ++i) body += "\u00E4";
    PB_CHECK_EQ(excerpt_of(body, 120).size(), static_cast<size_t>(240));
}

PB_TEST_MAIN()
