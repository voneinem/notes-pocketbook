// US4/US5 (spec 002 FR-019, research R-1): attachment references in the block parser.
#include <string>
#include <vector>

#include "pbnotes/markdown.h"
#include "test_support.h"

using pbnotes::Block;
using pbnotes::BlockKind;
using pbnotes::parse_markdown;
using pbnotes::Span;

namespace {

std::string plain(const std::vector<Span>& spans) {
    std::string s;
    for (const auto& sp : spans) s += sp.text;
    return s;
}
std::string plain(const Block& b) { return plain(b.spans); }

std::vector<Block> content_blocks(const std::string& text) {
    std::vector<Block> out;
    for (auto& b : parse_markdown(text))
        if (b.kind != BlockKind::Blank) out.push_back(b);
    return out;
}

}  // namespace

PB_TEST(an_image_reference_to_a_relative_path_is_an_attachment_block) {
    auto b = content_blocks("![my photo](.attachments.12/a%20b.png)");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
    PB_CHECK(b[0].kind == BlockKind::Attachment);
    PB_CHECK_EQ(b[0].attachment.path, std::string(".attachments.12/a b.png"));
    PB_CHECK_EQ(b[0].attachment.name, std::string("my photo"));
    PB_CHECK(b[0].attachment.is_image_syntax);
    PB_CHECK_EQ(b[0].attachment.token_id, static_cast<int64_t>(0));
    PB_CHECK_EQ(b[0].line_index, 0);
}

PB_TEST(a_relative_path_outside_the_attachment_folder_is_an_image_reference_too) {
    auto b = content_blocks("![](../Images/a.png)");
    PB_CHECK(b[0].kind == BlockKind::Attachment);
    PB_CHECK_EQ(b[0].attachment.path, std::string("../Images/a.png"));
    PB_CHECK_EQ(b[0].attachment.name, std::string());
}

PB_TEST(a_link_to_the_attachment_folder_is_an_attachment) {
    auto b = content_blocks("[the file](.attachments.12/f.pdf)");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
    PB_CHECK(b[0].kind == BlockKind::Attachment);
    PB_CHECK_EQ(b[0].attachment.path, std::string(".attachments.12/f.pdf"));
    PB_CHECK_EQ(b[0].attachment.name, std::string("the file"));
    PB_CHECK(!b[0].attachment.is_image_syntax);
}

PB_TEST(other_relative_links_are_plain_labels) {
    auto b = content_blocks("[page](docs/page.md) and [other](x.pdf)");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
    PB_CHECK(b[0].kind == BlockKind::Paragraph);
    PB_CHECK_EQ(plain(b[0]), std::string("page and other"));
}

PB_TEST(absolute_and_scheme_targets_are_not_attachments) {
    for (const char* text : {"![x](/x.png)", "![x](javascript:alert(1))", "![x](file:a.png)", "![x](C:/a.png)",
                             "![x](dir\\a.png)", "![x](a%00b.png)", "![x]()", "![x](dir/)"}) {
        for (const auto& x : parse_markdown(text)) PB_CHECK(x.kind != BlockKind::Attachment);
    }
}

PB_TEST(an_external_image_stays_a_link_not_an_attachment) {
    auto b = content_blocks("![alt](https://x.example/p.png)");
    PB_CHECK(b[0].kind == BlockKind::Paragraph);
    PB_CHECK_EQ(b[0].spans.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(b[0].spans[0].url, std::string("https://x.example/p.png"));
}

PB_TEST(a_local_token_is_an_attachment_with_its_id) {
    auto b = content_blocks("![photo.png](pbatt:7)");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
    PB_CHECK(b[0].kind == BlockKind::Attachment);
    PB_CHECK_EQ(b[0].attachment.token_id, static_cast<int64_t>(7));
    PB_CHECK_EQ(b[0].attachment.path, std::string());
    PB_CHECK_EQ(b[0].attachment.name, std::string("photo.png"));
}

PB_TEST(a_reference_in_a_sentence_splits_the_line_in_source_order) {
    auto all = parse_markdown("before ![x](.attachments.1/a.png) after\nnext");
    PB_CHECK_EQ(all.size(), static_cast<size_t>(4));
    PB_CHECK(all[0].kind == BlockKind::Paragraph);
    PB_CHECK_EQ(plain(all[0]), std::string("before"));
    PB_CHECK(all[1].kind == BlockKind::Attachment);
    PB_CHECK(all[2].kind == BlockKind::Paragraph);
    PB_CHECK_EQ(plain(all[2]), std::string("after"));
    PB_CHECK_EQ(all[0].line_index, 0);
    PB_CHECK_EQ(all[1].line_index, 0);
    PB_CHECK_EQ(all[2].line_index, 0);
    PB_CHECK_EQ(all[3].line_index, 1);
}

PB_TEST(two_references_on_one_line) {
    auto b = content_blocks("![a](.attachments.1/a.png)![b](pbatt:3) text");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK(b[0].kind == BlockKind::Attachment && b[0].attachment.path == ".attachments.1/a.png");
    PB_CHECK(b[1].kind == BlockKind::Attachment && b[1].attachment.token_id == 3);
    PB_CHECK(b[2].kind == BlockKind::Paragraph && plain(b[2]) == "text");
}

PB_TEST(a_reference_in_a_code_fence_stays_literal) {
    auto b = content_blocks("```\n![x](.attachments.1/a.png)\n```\n`![y](.attachments.1/b.png)` inline");
    for (const auto& x : b) PB_CHECK(x.kind != BlockKind::Attachment);
    PB_CHECK_EQ(plain(b[1]), std::string("![x](.attachments.1/a.png)"));
}

PB_TEST(references_in_tilde_fences_stay_literal_and_one_after_the_closer_is_found) {
    auto b = content_blocks("~~~\n![x](.attachments.1/a.png)\n~~~\n![y](.attachments.1/b.png)");
    int attachments = 0;
    for (const auto& x : b) attachments += x.kind == BlockKind::Attachment;
    PB_CHECK_EQ(attachments, 1);
    PB_CHECK_EQ(b[1].kind == BlockKind::Code, true);
    PB_CHECK_EQ(plain(b[1]), std::string("![x](.attachments.1/a.png)"));
    PB_CHECK_EQ(pbnotes::find_attachment_refs("~~~\n![x](.attachments.1/a.png)\n~~~\n![y](.attachments.1/b.png)").size(),
                static_cast<size_t>(1));
}

PB_TEST(a_reference_in_a_list_item_follows_the_item) {
    auto b = content_blocks("- item text ![x](.attachments.1/a.png)\n- [ ] task ![y](pbatt:2)");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(4));
    PB_CHECK(b[0].kind == BlockKind::ListItem);
    PB_CHECK_EQ(plain(b[0]), std::string("item text"));
    PB_CHECK(b[1].kind == BlockKind::Attachment);
    PB_CHECK(b[2].kind == BlockKind::ListItem && b[2].is_task);
    PB_CHECK_EQ(plain(b[2]), std::string("task"));
    PB_CHECK(b[3].kind == BlockKind::Attachment && b[3].attachment.token_id == 2);
    PB_CHECK_EQ(b[0].line_index, 0);
    PB_CHECK_EQ(b[1].line_index, 0);
    PB_CHECK_EQ(b[2].line_index, 1);
}

PB_TEST(a_reference_alone_in_a_heading_keeps_the_heading_text_first) {
    auto b = content_blocks("## Title ![x](.attachments.1/a.png)");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(2));
    PB_CHECK(b[0].kind == BlockKind::Heading);
    PB_CHECK_EQ(plain(b[0]), std::string("Title"));
    PB_CHECK(b[1].kind == BlockKind::Attachment);
}

PB_TEST(inside_a_table_cell_an_attachment_is_a_plain_text_name) {
    auto b = content_blocks("| a | b |\n|---|---|\n| ![pic](.attachments.1/a.png) | [doc](.attachments.1/d.pdf) |\n| ![](pbatt:4) | x |");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(1));
    PB_CHECK(b[0].kind == BlockKind::Table);
    PB_CHECK_EQ(plain(b[0].table.rows[1][0]), std::string("pic"));
    PB_CHECK_EQ(plain(b[0].table.rows[1][1]), std::string("doc"));
    PB_CHECK_EQ(plain(b[0].table.rows[2][0]), std::string());   // an empty name: nothing to show
    for (const auto& cell : b[0].table.rows[1]) for (const auto& sp : cell) PB_CHECK(sp.url.empty());
}

PB_TEST(find_attachment_refs_lists_every_reference_including_table_cells) {
    const std::string text = "![a](.attachments.1/a.png)\n| h |\n|---|\n| ![b](.attachments.1/b.png) |\n```\n![c](.attachments.1/c.png)\n```\n[d](.attachments.1/d.pdf) ![e](pbatt:9)";
    auto refs = pbnotes::find_attachment_refs(text);
    PB_CHECK_EQ(refs.size(), static_cast<size_t>(4));
    PB_CHECK_EQ(refs[0].path, std::string(".attachments.1/a.png"));
    PB_CHECK_EQ(refs[1].path, std::string(".attachments.1/b.png"));
    PB_CHECK_EQ(refs[2].path, std::string(".attachments.1/d.pdf"));
    PB_CHECK_EQ(refs[3].token_id, static_cast<int64_t>(9));
}

PB_TEST(plain_text_and_emphasis_around_references_still_parse) {
    auto b = content_blocks("**bold** ![x](.attachments.1/a.png) *it*");
    PB_CHECK_EQ(b.size(), static_cast<size_t>(3));
    PB_CHECK(b[0].spans[0].kind == Span::Bold);
    PB_CHECK(b[2].spans[0].kind == Span::Italic);
}

PB_TEST_MAIN()
