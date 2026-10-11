// Spec 006: the block and span rules of the formats the reading page shows. US2 (headings and bold) first; the quote,
// info box, details and inline strike / underline rules (US4) are added to this file by their own phase.
#include <string>
#include <vector>

#include "pbnotes/markdown.h"
#include "test_support.h"

using pbnotes::Block;
using pbnotes::BlockKind;
using pbnotes::parse_markdown;
using pbnotes::Span;

namespace {

std::string plain(const Block& b) {
    std::string s;
    for (const auto& sp : b.spans) s += sp.text;
    return s;
}

std::vector<Block> content(const std::string& text) {
    std::vector<Block> out;
    for (auto& b : parse_markdown(text))
        if (b.kind != BlockKind::Blank) out.push_back(b);
    return out;
}

bool has_bold(const Block& b, const std::string& text) {
    for (const auto& sp : b.spans)
        if (sp.kind == Span::Bold && sp.text == text) return true;
    return false;
}

}  // namespace

// ---- US2: headings and bold (the parser already does this; the tests pin it) -----------------------------------

PB_TEST(headings_of_every_level_are_headings_without_their_hashes) {
    const auto b = content("# A\n## B\n### C\n#### D\n##### E\n###### F");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(6));
    const char* texts[] = {"A", "B", "C", "D", "E", "F"};
    for (int i = 0; i < 6; ++i) {
        PB_CHECK(b[static_cast<size_t>(i)].kind == BlockKind::Heading);
        PB_CHECK_EQ(b[static_cast<size_t>(i)].level, i + 1);
        PB_CHECK_EQ(plain(b[static_cast<size_t>(i)]), std::string(texts[i]));
    }
}

PB_TEST(hashes_without_a_following_space_or_beyond_six_are_text) {
    const auto b = content("#hashtag\n####### seven\n#\n#\tx\n    # indented");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(5));
    for (const auto& x : b) PB_CHECK(x.kind != BlockKind::Heading);
    PB_CHECK_EQ(plain(b[0]), std::string("#hashtag"));
    PB_CHECK_EQ(plain(b[1]), std::string("####### seven"));
    PB_CHECK_EQ(plain(b[2]), std::string("#"));
}

PB_TEST(a_heading_inside_a_fence_is_code_not_a_heading) {
    const auto b = parse_markdown("```\n# not a heading\n```");
    PB_CHECK(b[1].kind == BlockKind::Code);
    PB_CHECK_EQ(plain(b[1]), std::string("# not a heading"));
}

PB_TEST(bold_is_bold_in_a_paragraph_a_heading_a_list_a_task_and_a_cell) {
    PB_CHECK(has_bold(content("a **b** c")[0], "b"));
    const auto h = content("# a **b** c");
    PB_CHECK(h[0].kind == BlockKind::Heading);
    PB_CHECK(has_bold(h[0], "b"));
    const auto l = content("- a **b** c");
    PB_CHECK(l[0].kind == BlockKind::ListItem);
    PB_CHECK(has_bold(l[0], "b"));
    const auto t = content("- [ ] a **b** c");
    PB_CHECK(t[0].is_task);
    PB_CHECK(has_bold(t[0], "b"));
    const auto tab = parse_markdown("| **b** | c |\n|---|---|\n| 1 | 2 |");
    PB_CHECK(tab[0].kind == BlockKind::Table);
    bool bold_cell = false;
    for (const auto& sp : tab[0].table.rows[0][0]) bold_cell = bold_cell || (sp.kind == Span::Bold && sp.text == "b");
    PB_CHECK(bold_cell);
    for (const auto& b : {h[0], l[0], t[0]}) {
        for (const auto& sp : b.spans) PB_CHECK(sp.text.find('*') == std::string::npos);  // no asterisks shown
    }
}

PB_TEST(an_unmatched_double_asterisk_is_literal_and_makes_nothing_bold) {
    for (const char* text : {"a ** b", "**open", "close**", "** spaced **"}) {
        const auto b = content(text);
        PB_CHECK_EQ(plain(b[0]), std::string(text));
        for (const auto& sp : b[0].spans) PB_CHECK(sp.kind == Span::Plain);
    }
}

PB_TEST(triple_asterisks_are_bold_italic_and_escaped_ones_are_literal) {
    const auto b = content("***both***");
    PB_CHECK_EQ(b[0].spans.size(), static_cast<size_t>(1));
    PB_CHECK(b[0].spans[0].kind == Span::BoldItalic);
    const auto e = content("\\*\\*x\\*\\*");
    PB_CHECK_EQ(plain(e[0]), std::string("**x**"));
    for (const auto& sp : e[0].spans) PB_CHECK(sp.kind == Span::Plain);
}

// ---- US4: inline strikethrough and underline --------------------------------------------------------------------

namespace {
const Span& only_span(const Block& b) {
    PB_CHECK_EQ(b.spans.size(), static_cast<size_t>(1));
    return b.spans[0];
}
}  // namespace

PB_TEST(strikethrough_and_underline_are_flags_on_the_text_without_their_marks) {
    const auto s = content("~~x~~");
    PB_CHECK_EQ(plain(s[0]), std::string("x"));
    PB_CHECK(only_span(s[0]).strike && !only_span(s[0]).underline);
    const auto u = content("<u>x</u>");
    PB_CHECK_EQ(plain(u[0]), std::string("x"));
    PB_CHECK(only_span(u[0]).underline && !only_span(u[0]).strike);
    const auto m = content("a ~~b~~ c");
    PB_CHECK_EQ(m[0].spans.size(), static_cast<size_t>(3));  // spans with different flags do not merge
    PB_CHECK(!m[0].spans[0].strike && m[0].spans[1].strike && !m[0].spans[2].strike);
    PB_CHECK_EQ(plain(m[0]), std::string("a b c"));
}

PB_TEST(the_flags_combine_with_bold_italic_and_links) {
    const auto b = content("**~~both~~**");
    PB_CHECK_EQ(b[0].spans.size(), static_cast<size_t>(1));
    PB_CHECK(b[0].spans[0].kind == Span::Bold && b[0].spans[0].strike);
    const auto c = content("~~a **b** c~~");
    PB_CHECK_EQ(c[0].spans.size(), static_cast<size_t>(3));
    for (const auto& sp : c[0].spans) PB_CHECK(sp.strike);
    PB_CHECK(c[0].spans[0].kind == Span::Plain && c[0].spans[1].kind == Span::Bold && c[0].spans[2].kind == Span::Plain);
    const auto l = content("~~a [link](https://x.example) b~~");
    bool found = false;
    for (const auto& sp : l[0].spans) {
        PB_CHECK(sp.strike);
        if (sp.url == "https://x.example") found = true;
    }
    PB_CHECK(found);
    const auto n = content("<u>a **b**</u> and *<u>c</u>*");
    bool bold_underlined = false, italic_underlined = false;
    for (const auto& sp : n[0].spans) {
        if (sp.kind == Span::Bold && sp.underline) bold_underlined = true;
        if (sp.kind == Span::Italic && sp.underline) italic_underlined = true;
    }
    PB_CHECK(bold_underlined && italic_underlined);
    const auto k = content("[~~gone~~ label](https://x.example)");
    PB_CHECK(!k[0].spans.empty() && k[0].spans[0].strike && k[0].spans[0].url == "https://x.example");
}

PB_TEST(unmatched_or_malformed_marks_stay_literal) {
    for (const char* text : {"~~x", "x~~", "~~ x ~~", "~x~", "a ~~~x~~~", "<u>x", "x</u>", "<u> </u>x", "<U>x</U>"}) {
        const auto b = content(text);
        PB_CHECK_EQ(plain(b[0]), std::string(text));
        for (const auto& sp : b[0].spans) PB_CHECK(!sp.strike && !sp.underline);
    }
}

PB_TEST(table_cells_keep_the_flags) {
    const auto b = parse_markdown("| ~~a~~ | <u>b</u> |\n|---|---|\n| 1 | 2 |");
    PB_CHECK(b[0].kind == BlockKind::Table);
    PB_CHECK(b[0].table.rows[0][0].size() == 1 && b[0].table.rows[0][0][0].strike);
    PB_CHECK(b[0].table.rows[0][1].size() == 1 && b[0].table.rows[0][1][0].underline);
}

PB_TEST(the_marks_of_strike_and_underline_are_part_of_the_source_extent) {
    auto b = parse_markdown("~~x~~");
    const Span& s = b[0].spans[0];
    PB_CHECK_EQ(s.src.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(s.src[0], 2);
    PB_CHECK_EQ(s.el_begin, 0);
    PB_CHECK_EQ(s.el_end, 5);
    b = parse_markdown("<u>x</u>");
    PB_CHECK_EQ(b[0].spans[0].src[0], 3);
    PB_CHECK_EQ(b[0].spans[0].el_begin, 0);
    PB_CHECK_EQ(b[0].spans[0].el_end, 8);
}

// ---- US4: quotes and info boxes ---------------------------------------------------------------------------------

PB_TEST(a_quote_line_is_a_block_inside_a_quote_group_with_its_marker_stripped) {
    for (const char* text : {"> a", ">a"}) {
        const auto b = parse_markdown(text);
        PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
        PB_CHECK(b[0].kind == BlockKind::Paragraph);
        PB_CHECK(b[0].quote == pbnotes::QuoteKind::Quote);
        PB_CHECK(b[0].quote_first && b[0].quote_last);
        PB_CHECK_EQ(plain(b[0]), std::string("a"));
        PB_CHECK_EQ(b[0].src_begin, 0);
        PB_CHECK_EQ(b[0].src_end, static_cast<int>(std::string(text).size()));
    }
    // up to three spaces of indent; four are not a quote
    PB_CHECK(parse_markdown("   > a")[0].quote == pbnotes::QuoteKind::Quote);
    PB_CHECK(parse_markdown("    > a")[0].quote == pbnotes::QuoteKind::None);
}

PB_TEST(blocks_inside_a_quote_keep_their_kind) {
    auto b = parse_markdown("> # H\n> - item\n> **bold** text");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK(b[0].kind == BlockKind::Heading && b[0].level == 1 && plain(b[0]) == "H");
    PB_CHECK(b[1].kind == BlockKind::ListItem && plain(b[1]) == "item");
    PB_CHECK(b[2].kind == BlockKind::Paragraph && has_bold(b[2], "bold"));
    for (const auto& x : b) PB_CHECK(x.quote == pbnotes::QuoteKind::Quote);
    // a table is not recognized in a quote
    for (const auto& x : parse_markdown("> | a | b |\n> |---|---|\n> | 1 | 2 |")) PB_CHECK(x.kind != BlockKind::Table);
}

PB_TEST(consecutive_quote_lines_form_one_group_and_a_gap_or_plain_line_ends_it) {
    auto b = parse_markdown("> a\n> b");
    PB_CHECK(b[0].quote_first && !b[0].quote_last);
    PB_CHECK(!b[1].quote_first && b[1].quote_last);
    b = parse_markdown("> a\n\n> b");  // a blank line between: two groups
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK(b[0].quote_first && b[0].quote_last);
    PB_CHECK(b[1].kind == BlockKind::Blank && b[1].quote == pbnotes::QuoteKind::None);
    PB_CHECK(b[2].quote_first && b[2].quote_last);
    b = parse_markdown("> a\n>\n> b");  // a lone ">" is a blank line inside the group
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK(b[1].kind == BlockKind::Blank && b[1].quote == pbnotes::QuoteKind::Quote);
    PB_CHECK(b[0].quote_first && !b[0].quote_last && !b[1].quote_first && !b[1].quote_last && b[2].quote_last);
    b = parse_markdown("> a\nplain");  // a line without ">" ends the quote
    PB_CHECK(b[0].quote_last && b[1].quote == pbnotes::QuoteKind::None);
}

PB_TEST(a_fence_opened_on_a_quote_line_is_a_code_box_inside_the_quote) {
    const auto b = parse_markdown("> ```yaml\n> a: 1\n> ```\nafter");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(4));
    PB_CHECK(b[0].kind == BlockKind::Fence && b[0].code_first && b[0].fence_info == "yaml");
    PB_CHECK(b[1].kind == BlockKind::Code && plain(b[1]) == "a: 1");
    PB_CHECK(b[2].kind == BlockKind::Fence && b[2].code_last);
    for (size_t i = 0; i < 3; ++i) PB_CHECK(b[i].quote == pbnotes::QuoteKind::Quote);
    PB_CHECK_EQ(b[1].src_begin, 0);
    PB_CHECK_EQ(b[1].src_end, 6);
    PB_CHECK_EQ(b[1].spans[0].src[0], 2);   // the code starts behind "> "
    PB_CHECK(b[3].kind == BlockKind::Paragraph && b[3].quote == pbnotes::QuoteKind::None);
    // an unclosed fence ends with the quote
    const auto u = parse_markdown("> ```\n> a\nplain");
    PB_CHECK(u[0].kind == BlockKind::Fence && u[1].kind == BlockKind::Code && u[1].code_last);
    PB_CHECK(u[2].kind == BlockKind::Paragraph && plain(u[2]) == "plain");
}

PB_TEST(an_info_header_makes_the_quote_an_info_box) {
    auto b = parse_markdown("> [!NOTE]\n> text\n> - item");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK(b[0].kind == BlockKind::Callout && b[0].callout == pbnotes::CalloutKind::Note);
    PB_CHECK(b[0].spans.empty());
    PB_CHECK_EQ(b[0].src_begin, 0);
    PB_CHECK_EQ(b[0].src_end, 9);
    for (const auto& x : b) PB_CHECK(x.quote == pbnotes::QuoteKind::Info);
    PB_CHECK(b[0].quote_first && !b[0].quote_last && b[2].quote_last);
    PB_CHECK(b[1].kind == BlockKind::Paragraph && b[2].kind == BlockKind::ListItem);
    struct K { const char* text; pbnotes::CalloutKind kind; };
    const K kinds[] = {{"> [!TIP]", pbnotes::CalloutKind::Tip}, {"> [!tip]", pbnotes::CalloutKind::Tip},
                       {"> [!Warning]  ", pbnotes::CalloutKind::Warning}, {"> [!IMPORTANT]", pbnotes::CalloutKind::Important},
                       {">[!caution]", pbnotes::CalloutKind::Caution}};
    for (const K& k : kinds) {
        const auto c = parse_markdown(k.text);
        PB_CHECK(c[0].kind == BlockKind::Callout && c[0].callout == k.kind);
    }
}

PB_TEST(only_the_first_line_of_a_quote_can_be_an_info_header_and_it_must_be_alone) {
    for (const char* text : {"> [!INFO]", "> [!NOTE] same line", "> text\n> [!NOTE]", "[!NOTE]"}) {
        for (const auto& x : parse_markdown(text)) PB_CHECK(x.kind != BlockKind::Callout);
    }
    const auto b = parse_markdown("> [!INFO]");
    PB_CHECK(b[0].quote == pbnotes::QuoteKind::Quote);
    PB_CHECK_EQ(plain(b[0]), std::string("[!INFO]"));
    const auto m = parse_markdown("> a\n> [!NOTE]");
    PB_CHECK(m[1].quote == pbnotes::QuoteKind::Quote);
}

// ---- US4: details sections --------------------------------------------------------------------------------------

PB_TEST(a_details_section_is_marker_summary_content_marker) {
    const auto b = parse_markdown("<details>\n<summary>S</summary>\nbody\n\n</details>\nafter");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(6));
    PB_CHECK(b[0].kind == BlockKind::Marker && b[0].details_id == -1);
    PB_CHECK(b[1].kind == BlockKind::Summary && plain(b[1]) == "S" && b[1].details_id == 0);
    PB_CHECK_EQ(b[1].src_begin, 0);
    PB_CHECK_EQ(b[1].src_end, 20);  // the whole "<summary>S</summary>" line
    PB_CHECK(b[2].kind == BlockKind::Paragraph && plain(b[2]) == "body" && b[2].details_id == 0);
    PB_CHECK(b[3].kind == BlockKind::Blank && b[3].details_id == 0);
    PB_CHECK(b[4].kind == BlockKind::Marker && b[4].details_id == -1);
    PB_CHECK(b[5].kind == BlockKind::Paragraph && b[5].details_id == -1);
    for (const auto& x : b) PB_CHECK(!x.details_open);
}

PB_TEST(without_a_summary_line_the_details_line_is_the_summary_with_the_text_Details) {
    const auto b = parse_markdown("<details>\nbody\n</details>");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK(b[0].kind == BlockKind::Summary && plain(b[0]) == "Details" && b[0].details_id == 0);
    PB_CHECK(b[1].details_id == 0);
    PB_CHECK(b[2].kind == BlockKind::Marker);
}

PB_TEST(details_open_and_the_case_of_the_tags) {
    auto b = parse_markdown("<details open>\n<summary>S</summary>\nx\n</details>");
    PB_CHECK(b[1].details_open && b[2].details_open);
    b = parse_markdown("<DETAILS OPEN>\n<SUMMARY>S</SUMMARY>\nx\n</DETAILS>");
    PB_CHECK(b[0].kind == BlockKind::Marker && b[1].kind == BlockKind::Summary && b[1].details_open);
    PB_CHECK(b[3].kind == BlockKind::Marker);
    b = parse_markdown("  <details>  \n<summary>S</summary>\nx\n</details>  ");
    PB_CHECK(b[0].kind == BlockKind::Marker);
}

PB_TEST(an_unclosed_section_runs_to_the_end_and_an_empty_or_formatted_summary_is_a_summary) {
    auto b = parse_markdown("<details>\n<summary>S</summary>\na\nb");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(4));
    PB_CHECK(b[2].details_id == 0 && b[3].details_id == 0);
    b = parse_markdown("<details>\n<summary></summary>\nx\n</details>");
    PB_CHECK(b[1].kind == BlockKind::Summary && b[1].spans.empty());
    b = parse_markdown("<details>\n<summary>**S** t</summary>\nx\n</details>");
    PB_CHECK(has_bold(b[1], "S"));
}

PB_TEST(nested_or_misplaced_details_lines_are_ordinary_text) {
    // a <details> inside a section is text; the first </details> ends the outer section
    auto b = parse_markdown("<details>\n<details>\nx\n</details>\ny");
    PB_CHECK(b[0].kind == BlockKind::Summary);  // no <summary> line: the outer line is the summary
    PB_CHECK(b[1].kind == BlockKind::Paragraph && plain(b[1]) == "<details>" && b[1].details_id == 0);
    PB_CHECK(b[3].kind == BlockKind::Marker);
    PB_CHECK(b[4].details_id == -1);
    // in a fence, in a quote, with text after the tag: not a section
    b = parse_markdown("```\n<details>\n```");
    PB_CHECK(b[1].kind == BlockKind::Code);
    b = parse_markdown("> <details>");
    PB_CHECK(b[0].kind == BlockKind::Paragraph && b[0].quote == pbnotes::QuoteKind::Quote);
    b = parse_markdown("<details> x\ny");
    PB_CHECK(b[0].kind == BlockKind::Paragraph && b[0].details_id == -1);
    // a <summary> that does not follow the <details> line directly is text
    b = parse_markdown("<details>\n\n<summary>S</summary>\n</details>");
    PB_CHECK(b[0].kind == BlockKind::Summary && plain(b[0]) == "Details");
    PB_CHECK(b[2].kind == BlockKind::Paragraph);
    // </details> inside a fence of the section is code
    b = parse_markdown("<details>\n<summary>S</summary>\n```\n</details>\n```\n</details>");
    PB_CHECK(b[3].kind == BlockKind::Code);
    PB_CHECK(b[5].kind == BlockKind::Marker);
    // a stray </details> outside a section is plain text
    b = parse_markdown("</details>");
    PB_CHECK(b[0].kind == BlockKind::Paragraph);
}

PB_TEST_MAIN()
