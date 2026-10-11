// Spec 006 US3 (research R-8, R-9; contracts/core-library.md): when a typed "/" opens the menu, what each entry
// inserts and where the caret goes. Offsets are UTF-16 code units. The date-time helpers of US5 and the re-parse
// checks of quote / info box / details (US4) are added to this file by their own phases.
#include <string>
#include <vector>

#include "pbnotes/markdown.h"
#include "pbnotes/slash.h"
#include "test_support.h"

using pbnotes::SlashDetected;
using pbnotes::SlashEdit;
using pbnotes::SlashEntry;
using pbnotes::SlashTrigger;
using pbnotes::detect_slash;
using pbnotes::expand_slash;
using pbnotes::kNoSlash;

namespace {

std::string u8(const std::u16string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        uint32_t c = s[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size()) c = 0x10000 + ((c - 0xD800) << 10) + (s[++i] - 0xDC00);
        if (c < 0x80) out += static_cast<char>(c);
        else if (c < 0x800) { out += static_cast<char>(0xC0 | (c >> 6)); out += static_cast<char>(0x80 | (c & 0x3F)); }
        else if (c < 0x10000) { out += static_cast<char>(0xE0 | (c >> 12)); out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)); out += static_cast<char>(0x80 | (c & 0x3F)); }
        else { out += static_cast<char>(0xF0 | (c >> 18)); out += static_cast<char>(0x80 | ((c >> 12) & 0x3F)); out += static_cast<char>(0x80 | ((c >> 6) & 0x3F)); out += static_cast<char>(0x80 | (c & 0x3F)); }
    }
    return out;
}

// "before" -> "after" with the caret behind the inserted "/" at p.
SlashDetected typed(const std::u16string& before, size_t p, size_t menu_pos = kNoSlash) {
    std::u16string after = before;
    after.insert(p, u"/");
    return detect_slash(before, after, p + 1, menu_pos);
}

std::u16string apply(const std::u16string& text, const SlashEdit& e) {
    std::u16string out = text;
    out.replace(e.begin, e.end - e.begin, e.text);
    return out;
}

SlashEdit expand_in(SlashEntry entry, const std::u16string& note, size_t slash_pos) {
    return expand_slash(entry, note, slash_pos, 1);
}

}  // namespace

PB_TEST(the_menu_has_twelve_entries_in_the_specified_order) {
    const auto& e = pbnotes::slash_entries();
    PB_CHECK_EQ(e.size(), static_cast<size_t>(12));
    const char* ids[] = {"bullet_list", "table", "info_box", "link", "bold", "italic",
                         "underline", "strikethrough", "details", "quote", "code_block", "emoji"};
    for (size_t i = 0; i < 12; ++i) PB_CHECK_EQ(std::string(e[i].id), std::string(ids[i]));
    PB_CHECK(e[0].entry == SlashEntry::BulletList && e[11].entry == SlashEntry::Emoji);
}

// ---- detect_slash ------------------------------------------------------------------------------

PB_TEST(a_slash_at_the_start_or_after_a_line_break_space_or_tab_opens_the_menu) {
    auto d = typed(u"", 0);
    PB_CHECK(d.trigger == SlashTrigger::Menu && d.pos == 0);
    d = typed(u"a\n", 2);
    PB_CHECK(d.trigger == SlashTrigger::Menu && d.pos == 2);
    d = typed(u"a ", 2);
    PB_CHECK(d.trigger == SlashTrigger::Menu && d.pos == 2);
    d = typed(u"a\t", 2);
    PB_CHECK(d.trigger == SlashTrigger::Menu);
    d = typed(u"a b", 2);  // "a /b"
    PB_CHECK(d.trigger == SlashTrigger::Menu && d.pos == 2);
    d = typed(u"\xD83D\xDE00 ", 3);  // after an emoji and a space: positions are UTF-16 units
    PB_CHECK(d.trigger == SlashTrigger::Menu && d.pos == 3);
}

PB_TEST(a_slash_after_any_other_character_is_an_ordinary_character) {
    for (const std::u16string& before : {std::u16string(u"a"), std::u16string(u"1"), std::u16string(u"x:"),
                                         std::u16string(u"a."), std::u16string(u"and"), std::u16string(u"(")}) {
        PB_CHECK(typed(before, before.size()).trigger == SlashTrigger::None);
    }
    PB_CHECK(typed(u"ab", 1).trigger == SlashTrigger::None);  // "a/b"
    PB_CHECK(typed(u"http:/", 6).trigger == SlashTrigger::None);  // "http://"
}

PB_TEST(a_second_slash_after_the_one_that_opened_the_menu_is_the_date_time_trigger) {
    auto d = typed(u"a /", 3, 2);  // "a //", the menu is open on the slash at 2
    PB_CHECK(d.trigger == SlashTrigger::DateTime && d.pos == 3);
    d = typed(u"/", 1, 0);
    PB_CHECK(d.trigger == SlashTrigger::DateTime && d.pos == 1);
    // without an open menu a second slash is not special (it is after a non-boundary character)
    PB_CHECK(typed(u"/", 1).trigger == SlashTrigger::None);
    // the menu belongs to another slash
    PB_CHECK(typed(u"a / ", 4, 2).trigger == SlashTrigger::Menu);  // after a space: a new menu, not a date
}

PB_TEST(only_one_typed_slash_counts) {
    PB_CHECK(detect_slash(u"", u"//", 2, kNoSlash).trigger == SlashTrigger::None);       // a pasted pair
    PB_CHECK(detect_slash(u"a /", u"a ", 2, kNoSlash).trigger == SlashTrigger::None);    // a delete
    PB_CHECK(detect_slash(u"a ", u"a x", 3, kNoSlash).trigger == SlashTrigger::None);    // another character
    PB_CHECK(detect_slash(u"a ", u"a /", 2, kNoSlash).trigger == SlashTrigger::None);    // the caret is not behind it
    PB_CHECK(detect_slash(u"a  b", u"a / b", 3, kNoSlash).trigger == SlashTrigger::Menu);  // inside existing text
    PB_CHECK(detect_slash(u"ab", u"ab", 2, kNoSlash).trigger == SlashTrigger::None);     // no change
    PB_CHECK(detect_slash(u"a ", u"a /x/", 3, kNoSlash).trigger == SlashTrigger::None);  // after != before + one slash
}

// ---- expand_slash: the exact text and caret of every entry ---------------------------------------

PB_TEST(every_entry_inserts_its_markdown_with_the_caret_where_typing_continues) {
    struct Row { SlashEntry entry; const char* text; size_t caret; };
    const Row rows[] = {
        {SlashEntry::BulletList, "- ", 2},
        {SlashEntry::Table, "|  |  |\n| --- | --- |\n|  |  |", 2},
        {SlashEntry::InfoBox, "> [!NOTE]\n> ", 12},
        {SlashEntry::Link, "[](https://)", 1},
        {SlashEntry::Bold, "****", 2},
        {SlashEntry::Italic, "**", 1},
        {SlashEntry::Underline, "<u></u>", 3},
        {SlashEntry::Strikethrough, "~~~~", 2},
        {SlashEntry::Details, "<details>\n<summary></summary>\n\n</details>", 19},
        {SlashEntry::Quote, "> ", 2},
        {SlashEntry::CodeBlock, "```\n\n```", 4},
    };
    for (const Row& r : rows) {
        const SlashEdit e = expand_in(r.entry, u"/", 0);
        PB_CHECK_EQ(e.begin, static_cast<size_t>(0));
        PB_CHECK_EQ(e.end, static_cast<size_t>(1));
        PB_CHECK_EQ(u8(e.text), std::string(r.text));
        PB_CHECK_EQ(e.caret, r.caret);
    }
}

PB_TEST(the_caret_is_absolute_in_the_text_after_the_replacement) {
    const SlashEdit e = expand_in(SlashEntry::Bold, u"abc /", 4);
    PB_CHECK_EQ(u8(apply(u"abc /", e)), std::string("abc ****"));
    PB_CHECK_EQ(e.caret, static_cast<size_t>(6));  // between the two pairs of asterisks
}

PB_TEST(a_block_entry_after_text_on_the_line_starts_a_new_line) {
    for (SlashEntry entry : {SlashEntry::BulletList, SlashEntry::Table, SlashEntry::InfoBox, SlashEntry::Details,
                             SlashEntry::Quote, SlashEntry::CodeBlock}) {
        const SlashEdit start = expand_in(entry, u"/", 0);
        const SlashEdit after_text = expand_in(entry, u"abc /", 4);
        PB_CHECK_EQ(u8(after_text.text), "\n" + u8(start.text));       // a line break in front
        PB_CHECK_EQ(after_text.caret, start.caret + 4 + 1);            // the caret moved by the break and the prefix
        const SlashEdit after_blank = expand_in(entry, u"  /", 2);     // only blanks before it: no break
        PB_CHECK_EQ(u8(after_blank.text), u8(start.text));
        const SlashEdit after_newline = expand_in(entry, u"abc\n/", 4);
        PB_CHECK_EQ(u8(after_newline.text), u8(start.text));
    }
}

PB_TEST(a_multi_line_entry_before_text_on_the_line_gets_a_line_break_behind_it) {
    for (SlashEntry entry : {SlashEntry::Table, SlashEntry::InfoBox, SlashEntry::Details, SlashEntry::CodeBlock}) {
        const SlashEdit plain = expand_in(entry, u"/", 0);
        const SlashEdit before_text = expand_in(entry, u"/xyz", 0);
        PB_CHECK_EQ(u8(before_text.text), u8(plain.text) + "\n");
        PB_CHECK_EQ(before_text.caret, plain.caret);
    }
    // a list item or a quote prefixes the text that follows: no break
    PB_CHECK_EQ(u8(expand_in(SlashEntry::BulletList, u"/xyz", 0).text), std::string("- "));
    PB_CHECK_EQ(u8(expand_in(SlashEntry::Quote, u"/xyz", 0).text), std::string("> "));
    // inline entries never add breaks
    for (SlashEntry entry : {SlashEntry::Link, SlashEntry::Bold, SlashEntry::Italic, SlashEntry::Underline,
                             SlashEntry::Strikethrough}) {
        const SlashEdit plain = expand_in(entry, u"/", 0);
        PB_CHECK_EQ(u8(expand_in(entry, u"abc /xyz", 4).text), u8(plain.text));
    }
}

PB_TEST(expanding_the_emoji_entry_only_removes_the_slash_and_replace_puts_text_in) {
    const SlashEdit e = expand_in(SlashEntry::Emoji, u"a /", 2);
    PB_CHECK_EQ(e.begin, static_cast<size_t>(2));
    PB_CHECK_EQ(e.end, static_cast<size_t>(3));
    PB_CHECK(e.text.empty());
    PB_CHECK_EQ(e.caret, static_cast<size_t>(2));
    const SlashEdit r = pbnotes::replace_slash(2, 1, u"\xD83D\xDE42");  // one emoji, two UTF-16 units
    PB_CHECK_EQ(r.begin, static_cast<size_t>(2));
    PB_CHECK_EQ(r.end, static_cast<size_t>(3));
    PB_CHECK_EQ(r.caret, static_cast<size_t>(4));
}

// ---- what is inserted is what the reading page shows ---------------------------------------------

PB_TEST(inserted_markdown_is_read_back_as_the_matching_blocks) {
    {
        const auto text = apply(u"/", expand_in(SlashEntry::Table, u"/", 0));
        const auto b = pbnotes::parse_markdown(u8(text));
        PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
        PB_CHECK(b[0].kind == pbnotes::BlockKind::Table);
        PB_CHECK_EQ(b[0].line_count, 3);
        PB_CHECK_EQ(b[0].table.align.size(), static_cast<size_t>(2));
        PB_CHECK_EQ(b[0].table.rows.size(), static_cast<size_t>(2));  // the header and the one body row
    }
    {
        const auto text = apply(u"/", expand_in(SlashEntry::CodeBlock, u"/", 0));
        const auto b = pbnotes::parse_markdown(u8(text));
        PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
        PB_CHECK(b[0].kind == pbnotes::BlockKind::Fence && b[1].kind == pbnotes::BlockKind::Code &&
                 b[2].kind == pbnotes::BlockKind::Fence);
    }
    {
        const auto text = apply(u"/", expand_in(SlashEntry::BulletList, u"/", 0)) + u"x";
        const auto b = pbnotes::parse_markdown(u8(text));
        PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
        PB_CHECK(b[0].kind == pbnotes::BlockKind::ListItem);
    }
}

// ---- spec 006 US4: the other inserted formats are read back as what the reading page shows ----

PB_TEST(info_box_quote_details_underline_and_strike_texts_parse_as_their_formats) {
    {
        const auto text = apply(u"/", expand_in(SlashEntry::InfoBox, u"/", 0)) + u"x";
        const auto b = pbnotes::parse_markdown(u8(text));
        PB_CHECK_EQ(b.size(), static_cast<size_t>(2));
        PB_CHECK(b[0].kind == pbnotes::BlockKind::Callout && b[0].callout == pbnotes::CalloutKind::Note);
        PB_CHECK(b[1].kind == pbnotes::BlockKind::Paragraph && b[1].quote == pbnotes::QuoteKind::Info);
    }
    {
        const auto text = apply(u"/", expand_in(SlashEntry::Quote, u"/", 0)) + u"x";
        const auto b = pbnotes::parse_markdown(u8(text));
        PB_CHECK(b[0].kind == pbnotes::BlockKind::Paragraph && b[0].quote == pbnotes::QuoteKind::Quote);
    }
    {
        const auto text = apply(u"/", expand_in(SlashEntry::Details, u"/", 0));
        const auto b = pbnotes::parse_markdown(u8(text));
        PB_CHECK_EQ(b.size(), static_cast<size_t>(4));
        PB_CHECK(b[0].kind == pbnotes::BlockKind::Marker && b[1].kind == pbnotes::BlockKind::Summary &&
                 b[2].kind == pbnotes::BlockKind::Blank && b[3].kind == pbnotes::BlockKind::Marker);
        PB_CHECK(b[1].spans.empty());
        PB_CHECK_EQ(b[1].details_id, 0);
    }
    {
        // with typed content between the marks (the caret position is inside them)
        const SlashEdit u = expand_in(SlashEntry::Underline, u"/", 0);
        std::u16string text = apply(u"/", u);
        text.insert(u.caret, u"under");
        const auto b = pbnotes::parse_markdown(u8(text));
        PB_CHECK(b[0].spans.size() == 1 && b[0].spans[0].underline);
        const SlashEdit st = expand_in(SlashEntry::Strikethrough, u"/", 0);
        text = apply(u"/", st);
        text.insert(st.caret, u"gone");
        const auto c = pbnotes::parse_markdown(u8(text));
        PB_CHECK(c[0].spans.size() == 1 && c[0].spans[0].strike);
        const SlashEdit bo = expand_in(SlashEntry::Bold, u"/", 0);
        text = apply(u"/", bo);
        text.insert(bo.caret, u"strong");
        const auto d = pbnotes::parse_markdown(u8(text));
        PB_CHECK(d[0].spans.size() == 1 && d[0].spans[0].kind == pbnotes::Span::Bold);
        const SlashEdit it = expand_in(SlashEntry::Italic, u"/", 0);
        text = apply(u"/", it);
        text.insert(it.caret, u"slanted");
        const auto e = pbnotes::parse_markdown(u8(text));
        PB_CHECK(e[0].spans.size() == 1 && e[0].spans[0].kind == pbnotes::Span::Italic);
        const SlashEdit li = expand_in(SlashEntry::Link, u"/", 0);
        text = apply(u"/", li);
        text.insert(li.caret, u"label");
        text.replace(text.find(u"https://") + 8, 0, u"example.org");
        const auto f = pbnotes::parse_markdown(u8(text));
        PB_CHECK(f[0].spans.size() == 1 && f[0].spans[0].url == "https://example.org");
    }
}

// ---- spec 006 US5: the date-time text and the days of a month ----

PB_TEST(a_timestamp_is_year_month_day_hour_minute_zero_padded_in_24_hours) {
    PB_CHECK_EQ(u8(pbnotes::format_timestamp(2026, 10, 11, 14, 30)), std::string("2026-10-11 14:30"));
    PB_CHECK_EQ(u8(pbnotes::format_timestamp(2026, 1, 2, 3, 4)), std::string("2026-01-02 03:04"));
    PB_CHECK_EQ(u8(pbnotes::format_timestamp(1970, 1, 1, 0, 0)), std::string("1970-01-01 00:00"));
    PB_CHECK_EQ(u8(pbnotes::format_timestamp(2099, 12, 31, 23, 59)), std::string("2099-12-31 23:59"));
}

PB_TEST(a_timestamp_clamps_out_of_range_values_instead_of_rejecting_them) {
    PB_CHECK_EQ(u8(pbnotes::format_timestamp(2026, 13, 32, 24, 60)), std::string("2026-12-31 23:59"));
    PB_CHECK_EQ(u8(pbnotes::format_timestamp(1900, 0, 0, -1, -1)), std::string("1970-01-01 00:00"));
    PB_CHECK_EQ(u8(pbnotes::format_timestamp(2026, 2, 31, 0, 0)), std::string("2026-02-28 00:00"));   // no 31 February
    PB_CHECK_EQ(u8(pbnotes::format_timestamp(2024, 2, 31, 0, 0)), std::string("2024-02-29 00:00"));   // a leap year
    PB_CHECK_EQ(u8(pbnotes::format_timestamp(3000, 5, 5, 5, 5)), std::string("2099-05-05 05:05"));
}

PB_TEST(days_in_month_knows_the_calendar) {
    const int expected[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int m = 1; m <= 12; ++m) PB_CHECK_EQ(pbnotes::days_in_month(2026, m), expected[m - 1]);
    PB_CHECK_EQ(pbnotes::days_in_month(2024, 2), 29);
    PB_CHECK_EQ(pbnotes::days_in_month(2100, 2), 28);   // divisible by 100, not by 400
    PB_CHECK_EQ(pbnotes::days_in_month(2000, 2), 29);   // divisible by 400
    PB_CHECK_EQ(pbnotes::days_in_month(1900, 2), 28);   // clamped to 1970, which is not a leap year
    PB_CHECK_EQ(pbnotes::days_in_month(2026, 0), 31);   // month 0 clamps to January
    PB_CHECK_EQ(pbnotes::days_in_month(2026, 13), 31);  // month 13 clamps to December
}

PB_TEST_MAIN()
