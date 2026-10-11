// T045: parse_markdown — headings, emphasis, lists, task items (with source line index), links as
// label text; tables, code and images stay plain text; special characters are preserved.
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
std::vector<Block> content_blocks(const std::string& text) {
    std::vector<Block> out;
    for (auto& b : parse_markdown(text))
        if (b.kind != BlockKind::Blank) out.push_back(b);
    return out;
}
}  // namespace

PB_TEST(headings_have_levels_one_to_six) {
    auto b = content_blocks("# H1\n## H2\n###### H6\n####### too deep\n#nospace");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(5));
    PB_CHECK(b[0].kind == BlockKind::Heading && b[0].level == 1);
    PB_CHECK(b[1].kind == BlockKind::Heading && b[1].level == 2);
    PB_CHECK(b[2].kind == BlockKind::Heading && b[2].level == 6);
    PB_CHECK(b[3].kind == BlockKind::Paragraph);
    PB_CHECK_EQ(plain(b[3]), std::string("####### too deep"));
    PB_CHECK(b[4].kind == BlockKind::Paragraph);
    PB_CHECK_EQ(plain(b[0]), std::string("H1"));
}

PB_TEST(emphasis_spans) {
    auto b = content_blocks("plain **bold** and *italic* and ***both*** end");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
    const auto& s = b[0].spans;
    PB_CHECK_EQ(s.size(), static_cast<size_t>(7));
    PB_CHECK(s[0].kind == Span::Plain && s[0].text == "plain ");
    PB_CHECK(s[1].kind == Span::Bold && s[1].text == "bold");
    PB_CHECK(s[3].kind == Span::Italic && s[3].text == "italic");
    PB_CHECK(s[5].kind == Span::BoldItalic && s[5].text == "both");
    PB_CHECK(s[6].kind == Span::Plain && s[6].text == " end");
}

PB_TEST(underscore_emphasis_ignores_snake_case) {
    auto b = content_blocks("snake_case_name and _it_ and __b__");
    const auto& s = b[0].spans;
    PB_CHECK_EQ(plain(b[0]), std::string("snake_case_name and it and b"));
    bool italic = false, bold = false;
    for (const auto& sp : s) {
        if (sp.kind == Span::Italic && sp.text == "it") italic = true;
        if (sp.kind == Span::Bold && sp.text == "b") bold = true;
    }
    PB_CHECK(italic && bold);
}

PB_TEST(unmatched_delimiters_stay_literal) {
    auto b = content_blocks("2 * 3 = 6 and a*b and **open");
    PB_CHECK_EQ(plain(b[0]), std::string("2 * 3 = 6 and a*b and **open"));
    PB_CHECK_EQ(b[0].spans.size(), static_cast<size_t>(1));
}

PB_TEST(emphasis_inside_headings) {
    auto b = content_blocks("# **Bold** title");
    PB_CHECK(b[0].kind == BlockKind::Heading);
    PB_CHECK(b[0].spans[0].kind == Span::Bold);
    PB_CHECK_EQ(plain(b[0]), std::string("Bold title"));
}

PB_TEST(bullet_lists_with_nesting_depth) {
    auto b = content_blocks("- one\n  - nested\n    - deeper\n* star\n+ plus");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(5));
    const int depths[] = {0, 1, 2, 0, 0};
    for (int i = 0; i < 5; ++i) {
        PB_CHECK(b[i].kind == BlockKind::ListItem);
        PB_CHECK(!b[i].ordered);
        PB_CHECK_EQ(b[i].depth, depths[i]);
    }
    PB_CHECK_EQ(plain(b[1]), std::string("nested"));
}

PB_TEST(ordered_lists_keep_their_numbers) {
    auto b = content_blocks("1. first\n2) second\n10. tenth");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK(b[0].ordered && b[0].number == 1);
    PB_CHECK(b[1].ordered && b[1].number == 2);
    PB_CHECK(b[2].ordered && b[2].number == 10);
    PB_CHECK_EQ(plain(b[2]), std::string("tenth"));
}

PB_TEST(task_items_report_state_and_source_line) {
    auto b = parse_markdown("Title\n- [ ] todo\n- [x] done\n\n- [X] done too\n  - [ ] nested");
    std::vector<Block> tasks;
    for (auto& x : b)
        if (x.is_task) tasks.push_back(x);
    PB_CHECK_EQ(tasks.size(), static_cast<size_t>(4));
    PB_CHECK(!tasks[0].checked && tasks[0].line_index == 1);
    PB_CHECK(tasks[1].checked && tasks[1].line_index == 2);
    PB_CHECK(tasks[2].checked && tasks[2].line_index == 4);  // the blank line counts as a source line
    PB_CHECK(!tasks[3].checked && tasks[3].line_index == 5 && tasks[3].depth == 1);
    PB_CHECK_EQ(plain(tasks[0]), std::string("todo"));  // the marker is not part of the text
}

PB_TEST(things_that_look_like_tasks_but_are_not) {
    auto b = content_blocks("- [link](https://example.com)\n- [] empty\n- [ ]nospace\n- [y] other\n- plain");
    for (const auto& x : b) PB_CHECK(!x.is_task);
    PB_CHECK_EQ(plain(b[0]), std::string("link"));  // a link in a list item
}

PB_TEST(links_show_their_label_text) {
    auto b = content_blocks("see [the docs](https://example.com/x) now, and [**bold** link](https://a.example)");
    PB_CHECK_EQ(plain(b[0]), std::string("see the docs now, and bold link"));
}

PB_TEST(images_tables_and_code_blocks) {
    auto b = content_blocks("![alt](docs/i.png)\n| a | b |\n|---|---|\n| 1 | 2 |\n```\nint x = 1; // **not bold**\n```\ninline `code *x*` here");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(6));
    PB_CHECK(b[0].kind == BlockKind::Attachment);  // a relative image is an attachment reference (spec 002)
    PB_CHECK(b[1].kind == BlockKind::Table);  // pipe tables are drawn as tables (spec 002)
    PB_CHECK(b[2].kind == BlockKind::Fence);  // spec 006: the fence lines are the edges of the code box, not text
    PB_CHECK(b[2].spans.empty());
    PB_CHECK(b[3].kind == BlockKind::Code);
    PB_CHECK_EQ(plain(b[3]), std::string("int x = 1; // **not bold**"));
    PB_CHECK_EQ(b[3].spans.size(), static_cast<size_t>(1));  // no emphasis inside a code fence
    PB_CHECK(b[4].kind == BlockKind::Fence);
    PB_CHECK_EQ(plain(b[5]), std::string("inline `code *x*` here"));  // no emphasis inside inline code
}

PB_TEST(special_characters_are_preserved_for_the_renderer_to_escape) {
    auto b = content_blocks("a < b && c > d <b>x</b> & \"q\"");
    PB_CHECK_EQ(plain(b[0]), std::string("a < b && c > d <b>x</b> & \"q\""));
}

PB_TEST(blank_lines_and_crlf) {
    auto b = parse_markdown("a\r\n\r\nb\r\n");
    PB_CHECK(b.size() >= 3);
    PB_CHECK(b[0].kind == BlockKind::Paragraph && plain(b[0]) == "a" && b[0].line_index == 0);
    PB_CHECK(b[1].kind == BlockKind::Blank && b[1].line_index == 1);
    PB_CHECK(b[2].kind == BlockKind::Paragraph && plain(b[2]) == "b" && b[2].line_index == 2);
}

PB_TEST(empty_text_has_no_blocks) { PB_CHECK(parse_markdown("").empty()); }

PB_TEST_MAIN()
