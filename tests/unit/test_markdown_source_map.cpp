// Spec 005 FR-031: the parser records where every displayed byte came from in the source line (Span::src), the
// source extent of every formatted element (Span::el_begin/el_end) and of every block (Block::src_begin/src_end), so
// the reading page can copy the note's own text. The fields are additive: nothing else about the parse changes.
#include <string>
#include <vector>

#include "pbnotes/markdown.h"
#include "test_support.h"

using pbnotes::Block;
using pbnotes::BlockKind;
using pbnotes::Span;
using pbnotes::parse_markdown;

namespace {

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        std::string line = text.substr(pos, (nl == std::string::npos ? text.size() : nl) - pos);
        pos = nl == std::string::npos ? text.size() : nl + 1;
        while (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(line);
    }
    return out;
}

bool is_text_block(const Block& b) {
    return b.kind == BlockKind::Heading || b.kind == BlockKind::Paragraph || b.kind == BlockKind::ListItem;
}

// The invariants of data-model.md for every text block of `text`.
void check_invariants(const std::string& text) {
    const auto lines = split_lines(text);
    for (const Block& b : parse_markdown(text)) {
        if (!is_text_block(b)) continue;
        const std::string& line = lines.at(static_cast<size_t>(b.line_index));
        PB_CHECK(b.src_begin >= 0 && b.src_begin <= b.src_end);
        PB_CHECK(static_cast<size_t>(b.src_end) <= line.size());
        int prev = -1;
        for (const Span& sp : b.spans) {
            if (sp.src.empty()) continue;  // text the line does not hold (an image without a description): unknown
            PB_CHECK_EQ(sp.src.size(), sp.text.size());
            PB_CHECK(sp.el_begin >= b.src_begin && sp.el_end <= b.src_end);
            for (size_t i = 0; i < sp.src.size(); ++i) {
                const int s = sp.src[i];
                PB_CHECK(s >= 0 && static_cast<size_t>(s) < line.size());
                PB_CHECK(s >= prev);
                prev = s;
                PB_CHECK(s >= sp.el_begin && s < sp.el_end);
            }
        }
    }
}

std::vector<int> range(int from, int to) {  // from..to inclusive
    std::vector<int> v;
    for (int i = from; i <= to; ++i) v.push_back(i);
    return v;
}

}  // namespace

PB_TEST(plain_text_maps_to_itself) {
    const auto b = parse_markdown("plain text");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
    PB_CHECK(b[0].kind == BlockKind::Paragraph);
    PB_CHECK(b[0].spans[0].src == range(0, 9));
    PB_CHECK_EQ(b[0].src_begin, 0);
    PB_CHECK_EQ(b[0].src_end, 10);
    PB_CHECK_EQ(b[0].spans[0].el_begin, 0);
    PB_CHECK_EQ(b[0].spans[0].el_end, 10);
}

PB_TEST(a_heading_marker_is_part_of_the_block_but_not_of_the_text) {
    const auto b = parse_markdown("# Heading");
    PB_CHECK(b[0].kind == BlockKind::Heading);
    PB_CHECK_EQ(b[0].spans[0].text, std::string("Heading"));
    PB_CHECK(b[0].spans[0].src == range(2, 8));
    PB_CHECK_EQ(b[0].src_begin, 0);
    PB_CHECK_EQ(b[0].src_end, 9);
}

PB_TEST(list_and_task_prefixes_are_inside_the_block_extent) {
    auto b = parse_markdown("- item");
    PB_CHECK(b[0].spans[0].src == range(2, 5));
    PB_CHECK_EQ(b[0].src_end, 6);
    b = parse_markdown("  - [ ] task");
    PB_CHECK(b[0].is_task);
    PB_CHECK_EQ(b[0].spans[0].text, std::string("task"));
    PB_CHECK(b[0].spans[0].src == range(8, 11));
    PB_CHECK_EQ(b[0].src_begin, 0);
    PB_CHECK_EQ(b[0].src_end, 12);
    b = parse_markdown("1. one");
    PB_CHECK(b[0].spans[0].src == range(3, 5));
}

PB_TEST(bold_text_knows_its_marks) {
    const auto b = parse_markdown("**bold** text");
    PB_CHECK_EQ(b[0].spans.size(), static_cast<size_t>(2));
    const Span& bold = b[0].spans[0];
    PB_CHECK(bold.kind == Span::Bold);
    PB_CHECK_EQ(bold.text, std::string("bold"));
    PB_CHECK(bold.src == range(2, 5));
    PB_CHECK_EQ(bold.el_begin, 0);
    PB_CHECK_EQ(bold.el_end, 8);
    const Span& plain = b[0].spans[1];
    PB_CHECK(plain.src == range(8, 12));
    PB_CHECK_EQ(plain.el_begin, 8);
    PB_CHECK_EQ(plain.el_end, 13);
}

PB_TEST(italic_and_bold_italic_marks) {
    auto b = parse_markdown("*it*");
    PB_CHECK(b[0].spans[0].src == range(1, 2));
    PB_CHECK_EQ(b[0].spans[0].el_end, 4);
    b = parse_markdown("_it_");
    PB_CHECK(b[0].spans[0].src == range(1, 2));
    b = parse_markdown("***bi***");
    PB_CHECK(b[0].spans[0].kind == Span::BoldItalic);
    PB_CHECK(b[0].spans[0].src == range(3, 4));
    PB_CHECK_EQ(b[0].spans[0].el_begin, 0);
    PB_CHECK_EQ(b[0].spans[0].el_end, 8);
}

PB_TEST(a_markdown_link_covers_label_and_address) {
    const auto b = parse_markdown("a [label](https://example.org/x) b");
    PB_CHECK_EQ(b[0].spans.size(), static_cast<size_t>(3));
    const Span& link = b[0].spans[1];
    PB_CHECK_EQ(link.text, std::string("label"));
    PB_CHECK(!link.url.empty());
    PB_CHECK(link.src == range(3, 7));
    PB_CHECK_EQ(link.el_begin, 2);
    PB_CHECK_EQ(link.el_end, 32);
    PB_CHECK(b[0].spans[0].src == range(0, 1));
    PB_CHECK(b[0].spans[2].src == range(32, 33));
}

PB_TEST(autolinks_and_bare_addresses) {
    auto b = parse_markdown("<https://example.org/a>");
    PB_CHECK_EQ(b[0].spans[0].text, std::string("https://example.org/a"));
    PB_CHECK_EQ(b[0].spans[0].src[0], 1);
    PB_CHECK_EQ(b[0].spans[0].el_begin, 0);
    PB_CHECK_EQ(b[0].spans[0].el_end, 23);
    b = parse_markdown("see https://example.org/a b");
    PB_CHECK_EQ(b[0].spans.size(), static_cast<size_t>(3));
    PB_CHECK_EQ(b[0].spans[1].src[0], 4);
    PB_CHECK_EQ(b[0].spans[1].el_begin, 4);
    PB_CHECK_EQ(b[0].spans[1].el_end, 25);
}

PB_TEST(an_escaped_character_points_at_its_backslash) {
    const auto b = parse_markdown("\\*star\\*");
    PB_CHECK_EQ(b[0].spans[0].text, std::string("*star*"));
    PB_CHECK((b[0].spans[0].src == std::vector<int>{0, 2, 3, 4, 5, 6}));
    PB_CHECK_EQ(b[0].spans[0].el_begin, 0);
    PB_CHECK_EQ(b[0].spans[0].el_end, 8);
}

PB_TEST(inline_code_is_kept_verbatim) {
    const auto b = parse_markdown("`a*b*`");
    PB_CHECK_EQ(b[0].spans[0].text, std::string("`a*b*`"));
    PB_CHECK(b[0].spans[0].src == range(0, 5));
}

PB_TEST(a_link_inside_bold_keeps_the_outer_element_extent) {
    const std::string line = "**bold [l](https://example.org) x**";
    const auto b = parse_markdown(line);
    bool saw_link = false;
    for (const Span& sp : b[0].spans) {
        PB_CHECK_EQ(sp.el_begin, 0);
        PB_CHECK_EQ(sp.el_end, static_cast<int>(line.size()));
        if (!sp.url.empty()) { saw_link = true; PB_CHECK_EQ(sp.text, std::string("l")); }
    }
    PB_CHECK(saw_link);
}

PB_TEST(multibyte_characters_map_byte_by_byte) {
    const auto b = parse_markdown("\xC3\xA4 **\xC3\xB6**");  // "ä **ö**"
    PB_CHECK_EQ(b[0].spans.size(), static_cast<size_t>(2));
    PB_CHECK((b[0].spans[0].src == std::vector<int>{0, 1, 2}));
    PB_CHECK((b[0].spans[1].src == std::vector<int>{5, 6}));
    PB_CHECK_EQ(b[0].spans[1].el_begin, 3);
    PB_CHECK_EQ(b[0].spans[1].el_end, 9);
}

PB_TEST(text_after_an_attachment_reference_knows_its_place_in_the_line) {
    const auto b = parse_markdown("see ![n](pbatt:3) after");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK_EQ(b[0].spans[0].text, std::string("see"));
    PB_CHECK_EQ(b[0].src_begin, 0);
    PB_CHECK_EQ(b[0].src_end, 3);
    PB_CHECK(b[1].kind == BlockKind::Attachment);
    PB_CHECK_EQ(b[1].src_begin, 4);
    PB_CHECK_EQ(b[1].src_end, 17);
    PB_CHECK_EQ(b[2].spans[0].text, std::string("after"));
    PB_CHECK(b[2].spans[0].src == range(18, 22));
    PB_CHECK_EQ(b[2].src_begin, 18);
    PB_CHECK_EQ(b[2].src_end, 23);
}

PB_TEST(code_fences_and_very_long_lines_map_verbatim) {
    auto b = parse_markdown("```\ncode *x*\n```");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK_EQ(b[1].spans[0].text, std::string("code *x*"));
    PB_CHECK(b[1].spans[0].src == range(0, 7));
    const std::string big(20000, 'a');
    b = parse_markdown(big);
    PB_CHECK_EQ(b[0].spans[0].src.size(), big.size());
    PB_CHECK_EQ(b[0].src_end, 20000);
}

PB_TEST(tables_and_blank_lines_are_left_alone) {
    const auto b = parse_markdown("| a | b |\n|---|---|\n| 1 | 2 |\n\ntext");
    PB_CHECK(b[0].kind == BlockKind::Table);
    PB_CHECK_EQ(b[0].line_count, 3);
    PB_CHECK(b[1].kind == BlockKind::Blank);
    PB_CHECK_EQ(b[1].src_begin, 0);
    PB_CHECK_EQ(b[1].src_end, 0);
    PB_CHECK(b[2].kind == BlockKind::Paragraph);
}

PB_TEST(shown_text_that_is_not_in_the_line_has_no_offsets_but_an_extent) {
    const auto b = parse_markdown("![](https://example.org/i.png) after");
    PB_CHECK(b[0].spans[0].src.empty());
    PB_CHECK_EQ(b[0].spans[0].el_begin, 0);
    PB_CHECK_EQ(b[0].spans[0].el_end, 30);
}

PB_TEST(the_invariants_hold_for_a_mixed_corpus) {
    for (const char* text : {
             "plain text", "# Heading **b**", "- item with [a link](https://example.org) and *it*",
             "  - [x] done _soft_ and `code`", "1. one\n2. two", "**bold [l](https://example.org) x**",
             "\\*star\\* and \\_under\\_", "see ![n](pbatt:3) after and [f](.attachments.2/f.pdf) more",
             "<https://example.org/a> and https://example.org/b.", "\xC3\xA4 **\xC3\xB6** \xF0\x9F\x98\x80 *x*",
             "```\n**not bold**\n```", "***both*** and **bold *nested* bold**", "a \\\\ b \\[x\\]",
             "[unfollowable](javascript:alert(1)) text", "![alt](https://example.org/i.png) after",
             "![](https://example.org/i.png)", "[outer [inner](https://example.org) tail](https://example.org/o)",
             "trailing spaces   ", "* star list", "   indented paragraph"})
        check_invariants(text);
}

PB_TEST_MAIN()
