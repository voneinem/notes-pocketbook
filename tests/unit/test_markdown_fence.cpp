// Spec 006 US1 (research R-1, R-2): fenced code blocks. A fence line is a Fence block, a line inside it a Code block
// (one literal span), the first / last block of a box carries code_first / code_last; the one fence rule is shared by
// the attachment finder and the tokenizer.
#include <map>
#include <string>
#include <vector>

#include "pbnotes/markdown.h"
#include "test_support.h"

using pbnotes::Block;
using pbnotes::BlockKind;
using pbnotes::parse_markdown;

namespace {

std::string plain(const Block& b) {
    std::string s;
    for (const auto& sp : b.spans) s += sp.text;
    return s;
}

}  // namespace

PB_TEST(a_yaml_fence_is_fence_code_fence_with_the_text_exactly) {
    const auto b = parse_markdown("```yaml\na:\n  b: 1\n```");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(4));
    PB_CHECK(b[0].kind == BlockKind::Fence);
    PB_CHECK_EQ(b[0].fence_info, std::string("yaml"));
    PB_CHECK(b[0].code_first && !b[0].code_last);
    PB_CHECK(b[0].spans.empty());
    PB_CHECK(b[1].kind == BlockKind::Code && b[2].kind == BlockKind::Code);
    PB_CHECK_EQ(plain(b[1]), std::string("a:"));
    PB_CHECK_EQ(plain(b[2]), std::string("  b: 1"));  // both spaces kept
    PB_CHECK_EQ(b[2].spans.size(), static_cast<size_t>(1));
    PB_CHECK(!b[1].code_first && !b[1].code_last);
    PB_CHECK(b[3].kind == BlockKind::Fence);
    PB_CHECK(b[3].code_last && !b[3].code_first);
    for (size_t i = 0; i < b.size(); ++i) PB_CHECK_EQ(b[i].line_index, static_cast<int>(i));
    PB_CHECK_EQ(b[0].src_begin, 0);
    PB_CHECK_EQ(b[0].src_end, 7);   // "```yaml"
    PB_CHECK_EQ(b[2].src_begin, 0);
    PB_CHECK_EQ(b[2].src_end, 6);
    PB_CHECK_EQ(b[3].src_end, 3);
}

PB_TEST(markup_inside_a_fence_stays_literal) {
    const auto b = parse_markdown(
        "```\n**not bold**\n# not a heading\n- item\n| a | b |\n[a](https://b.example)\n![x](.attachments.1/a.png)\n```");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(8));
    for (size_t i = 1; i <= 6; ++i) {
        PB_CHECK(b[i].kind == BlockKind::Code);
        PB_CHECK_EQ(b[i].spans.size(), static_cast<size_t>(1));
        PB_CHECK(b[i].spans[0].kind == pbnotes::Span::Plain);
        PB_CHECK(b[i].spans[0].url.empty());
    }
    PB_CHECK_EQ(plain(b[1]), std::string("**not bold**"));
    PB_CHECK_EQ(plain(b[2]), std::string("# not a heading"));
    PB_CHECK_EQ(plain(b[5]), std::string("[a](https://b.example)"));
    PB_CHECK_EQ(plain(b[6]), std::string("![x](.attachments.1/a.png)"));
}

PB_TEST(tilde_fences_work_like_backtick_fences) {
    const auto b = parse_markdown("~~~\ncode\n~~~");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK(b[0].kind == BlockKind::Fence && b[1].kind == BlockKind::Code && b[2].kind == BlockKind::Fence);
    const auto four = parse_markdown("~~~~\nx\n~~~~");
    PB_CHECK(four[0].kind == BlockKind::Fence && four[2].kind == BlockKind::Fence);
}

PB_TEST(a_closer_must_be_as_long_and_of_the_same_kind) {
    // four backticks are not closed by three, but by five
    auto b = parse_markdown("````\na\n```\nb\n`````\nafter");
    PB_CHECK(b[0].kind == BlockKind::Fence);
    PB_CHECK(b[1].kind == BlockKind::Code && b[2].kind == BlockKind::Code && b[3].kind == BlockKind::Code);
    PB_CHECK_EQ(plain(b[2]), std::string("```"));
    PB_CHECK(b[4].kind == BlockKind::Fence && b[4].code_last);
    PB_CHECK(b[5].kind == BlockKind::Paragraph);
    // a backtick fence is not closed by tildes
    b = parse_markdown("```\na\n~~~\nb");
    PB_CHECK(b[2].kind == BlockKind::Code);
    PB_CHECK(b.back().kind == BlockKind::Code && b.back().code_last);
}

PB_TEST(a_closer_may_have_trailing_spaces_but_no_text) {
    auto b = parse_markdown("```\na\n```   \nafter");
    PB_CHECK(b[2].kind == BlockKind::Fence);
    PB_CHECK(b[3].kind == BlockKind::Paragraph);
    b = parse_markdown("```\na\n``` text\nb\n```");
    PB_CHECK(b[2].kind == BlockKind::Code);  // not a closer
    PB_CHECK_EQ(plain(b[2]), std::string("``` text"));
    PB_CHECK(b[4].kind == BlockKind::Fence);
}

PB_TEST(indent_of_up_to_three_spaces_still_opens_a_fence) {
    auto b = parse_markdown("   ```\ncode\n   ```");
    PB_CHECK(b[0].kind == BlockKind::Fence && b[2].kind == BlockKind::Fence);
    PB_CHECK_EQ(b[0].src_begin, 0);
    PB_CHECK_EQ(b[0].src_end, 6);
    b = parse_markdown("    ```\ncode");
    PB_CHECK(b[0].kind == BlockKind::Paragraph);
    PB_CHECK(b[1].kind == BlockKind::Paragraph);
    b = parse_markdown("\t```\ncode");
    PB_CHECK(b[0].kind == BlockKind::Paragraph);
}

PB_TEST(a_backtick_in_the_info_string_of_a_backtick_fence_is_not_a_fence) {
    const auto b = parse_markdown("```a`b\ntext");
    PB_CHECK(b[0].kind == BlockKind::Paragraph);
    PB_CHECK(b[1].kind == BlockKind::Paragraph);
    // tilde fences may have backticks in the info string
    const auto t = parse_markdown("~~~a`b\ntext\n~~~");
    PB_CHECK(t[0].kind == BlockKind::Fence);
}

PB_TEST(an_empty_fence_has_no_code_between_its_edges) {
    const auto b = parse_markdown("```\n```");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(2));
    PB_CHECK(b[0].kind == BlockKind::Fence && b[0].code_first && !b[0].code_last);
    PB_CHECK(b[1].kind == BlockKind::Fence && b[1].code_last && !b[1].code_first);
}

PB_TEST(an_unclosed_fence_runs_to_the_end_of_the_note) {
    auto b = parse_markdown("before\n```\na\n\nb");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(5));
    PB_CHECK(b[0].kind == BlockKind::Paragraph);
    PB_CHECK(b[1].kind == BlockKind::Fence && b[1].code_first);
    PB_CHECK(b[2].kind == BlockKind::Code && !b[2].code_last);
    PB_CHECK(b[3].kind == BlockKind::Code);  // a blank line inside a fence stays code, the box stays unbroken
    PB_CHECK(b[3].spans.empty());
    PB_CHECK(b[4].kind == BlockKind::Code && b[4].code_last);
    // an unclosed empty fence: the opener is the whole box
    b = parse_markdown("```");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
    PB_CHECK(b[0].code_first && b[0].code_last);
}

PB_TEST(a_fence_needs_no_blank_line_before_it_and_two_fences_are_independent) {
    const auto b = parse_markdown("para\n```\na\n```\nmid\n~~~\nb\n~~~");
    PB_CHECK(b[0].kind == BlockKind::Paragraph);
    PB_CHECK(b[1].kind == BlockKind::Fence && b[1].code_first);
    PB_CHECK(b[3].kind == BlockKind::Fence && b[3].code_last);
    PB_CHECK(b[4].kind == BlockKind::Paragraph);
    PB_CHECK(b[5].kind == BlockKind::Fence && b[5].code_first);
    PB_CHECK(b[7].kind == BlockKind::Fence && b[7].code_last);
}

PB_TEST(a_very_long_code_line_is_literal_in_full) {
    const std::string big(20000, 'z');
    const auto b = parse_markdown("```\n" + big + "\n```");
    PB_CHECK(b[1].kind == BlockKind::Code);
    PB_CHECK_EQ(plain(b[1]).size(), static_cast<size_t>(20000));
    PB_CHECK_EQ(b[1].src_end, 20000);
}

PB_TEST(attachment_references_and_tokens_ignore_the_same_fences) {
    const std::string text =
        "![a](.attachments.1/a.png)\n~~~\n![b](.attachments.1/b.png)\n~~~\n   ```\n![c](.attachments.1/c.png)\n   ```\n"
        "    ```\n![d](.attachments.1/d.png)\n    ```\n";
    const auto refs = pbnotes::find_attachment_refs(text);
    // a, d (the four-space "fence" is not one; both its marker lines and d are text) -- b and c are inside fences
    std::vector<std::string> paths;
    for (const auto& r : refs) paths.push_back(r.path);
    PB_CHECK_EQ(paths.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(paths[0], std::string(".attachments.1/a.png"));
    PB_CHECK_EQ(paths[1], std::string(".attachments.1/d.png"));
    const std::map<std::string, int64_t> tokens = {{".attachments.1/a.png", 1}, {".attachments.1/b.png", 2},
                                                   {".attachments.1/c.png", 3}, {".attachments.1/d.png", 4}};
    const std::string out = pbnotes::tokenize_attachment_refs(text, tokens);
    PB_CHECK(out.find("![b](.attachments.1/b.png)") != std::string::npos);  // untouched inside ~~~
    PB_CHECK(out.find("![c](.attachments.1/c.png)") != std::string::npos);  // untouched inside the indented fence
    PB_CHECK(out.find("pbatt:1") != std::string::npos);
    PB_CHECK(out.find("pbatt:4") != std::string::npos);
}

PB_TEST_MAIN()
