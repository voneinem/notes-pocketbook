// US1 (spec 002 FR-001..FR-004): links in the inline parser — titled links, bare addresses, autolinks,
// external images as links, non-followable targets as plain label text.
#include <string>
#include <vector>

#include "pbnotes/markdown.h"
#include "test_support.h"

using pbnotes::Block;
using pbnotes::BlockKind;
using pbnotes::parse_markdown;
using pbnotes::Span;

namespace {
std::vector<Block> content_blocks(const std::string& text) {
    std::vector<Block> out;
    for (auto& b : parse_markdown(text))
        if (b.kind != BlockKind::Blank) out.push_back(b);
    return out;
}
std::string plain(const Block& b) {
    std::string s;
    for (const auto& sp : b.spans) s += sp.text;
    return s;
}
std::vector<Span> links_of(const Block& b) {
    std::vector<Span> out;
    for (const auto& sp : b.spans)
        if (!sp.url.empty()) out.push_back(sp);
    return out;
}
}  // namespace

PB_TEST(titled_link_keeps_its_target_and_asks_for_confirmation) {
    auto b = content_blocks("see [Read this](https://example.org/a?b=1) now");
    PB_CHECK_EQ(plain(b[0]), std::string("see Read this now"));
    auto l = links_of(b[0]);
    PB_CHECK_EQ(l.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(l[0].text, std::string("Read this"));
    PB_CHECK_EQ(l[0].url, std::string("https://example.org/a?b=1"));
    PB_CHECK(l[0].link_confirm);
}

PB_TEST(emphasis_inside_the_link_text_is_kept) {
    auto b = content_blocks("[a **bold** word](https://example.org)");
    auto& s = b[0].spans;
    PB_CHECK_EQ(s.size(), static_cast<size_t>(3));
    PB_CHECK(s[0].kind == Span::Plain && s[0].text == "a " && s[0].url == "https://example.org");
    PB_CHECK(s[1].kind == Span::Bold && s[1].text == "bold" && s[1].url == "https://example.org");
    PB_CHECK(s[2].kind == Span::Plain && s[2].text == " word" && s[2].url == "https://example.org");
}

PB_TEST(a_bare_address_becomes_a_link_whose_text_is_the_address) {
    auto b = content_blocks("Plain address: https://example.org/path/page and more");
    auto l = links_of(b[0]);
    PB_CHECK_EQ(l.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(l[0].text, std::string("https://example.org/path/page"));
    PB_CHECK_EQ(l[0].url, std::string("https://example.org/path/page"));
    PB_CHECK(!l[0].link_confirm);
    PB_CHECK_EQ(plain(b[0]), std::string("Plain address: https://example.org/path/page and more"));
}

PB_TEST(a_link_whose_text_is_its_address_needs_no_confirmation) {
    auto b = content_blocks("[https://example.org](https://example.org)");
    auto l = links_of(b[0]);
    PB_CHECK_EQ(l.size(), static_cast<size_t>(1));
    PB_CHECK(!l[0].link_confirm);
}

PB_TEST(trailing_punctuation_is_not_part_of_a_bare_address) {
    for (const char* tail : {".", ",", ";", ":", "!", "?", ")"}) {
        auto b = content_blocks(std::string("go to https://example.org/x") + tail + " ok");
        auto l = links_of(b[0]);
        PB_CHECK_EQ(l.size(), static_cast<size_t>(1));
        PB_CHECK_EQ(l[0].url, std::string("https://example.org/x"));
        PB_CHECK_EQ(plain(b[0]), std::string("go to https://example.org/x") + tail + " ok");
    }
}

PB_TEST(a_balanced_parenthesis_stays_in_a_bare_address) {
    auto b = content_blocks("see https://en.wikipedia.org/wiki/A_(b) for details");
    auto l = links_of(b[0]);
    PB_CHECK_EQ(l.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(l[0].url, std::string("https://en.wikipedia.org/wiki/A_(b)"));
    auto c = content_blocks("(see https://example.org/x)");
    auto m = links_of(c[0]);
    PB_CHECK_EQ(m.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(m[0].url, std::string("https://example.org/x"));
    PB_CHECK_EQ(plain(c[0]), std::string("(see https://example.org/x)"));
}

PB_TEST(an_autolink_in_angle_brackets_is_a_link) {
    auto b = content_blocks("mail <https://x.example/a> done");
    auto l = links_of(b[0]);
    PB_CHECK_EQ(l.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(l[0].url, std::string("https://x.example/a"));
    PB_CHECK_EQ(l[0].text, std::string("https://x.example/a"));
    PB_CHECK_EQ(plain(b[0]), std::string("mail https://x.example/a done"));
}

PB_TEST(a_link_title_is_ignored) {
    auto b = content_blocks("[a](https://x.example \"the title\") and [b](https://y.example 'single')");
    auto l = links_of(b[0]);
    PB_CHECK_EQ(l.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(l[0].url, std::string("https://x.example"));
    PB_CHECK_EQ(l[1].url, std::string("https://y.example"));
    PB_CHECK_EQ(plain(b[0]), std::string("a and b"));
}

PB_TEST(targets_that_cannot_be_followed_render_as_their_label) {
    auto b = content_blocks("[write](mailto:someone@example.org) [rel](docs/page.md) [js](javascript:alert(1)) [ftp](ftp://x.example/f)");
    PB_CHECK(links_of(b[0]).empty());
    PB_CHECK_EQ(plain(b[0]), std::string("write rel js ftp"));
}

PB_TEST(an_external_image_becomes_a_link_with_the_alt_text) {
    auto b = content_blocks("![A photo](https://x.example/p.png)");
    auto l = links_of(b[0]);
    PB_CHECK_EQ(l.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(l[0].text, std::string("A photo"));
    PB_CHECK_EQ(l[0].url, std::string("https://x.example/p.png"));
    PB_CHECK(l[0].link_confirm);
}

PB_TEST(an_external_image_without_alt_text_shows_the_address) {
    auto b = content_blocks("![](https://x.example/p.png)");
    auto l = links_of(b[0]);
    PB_CHECK_EQ(l.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(l[0].text, std::string("https://x.example/p.png"));
    PB_CHECK(!l[0].link_confirm);
}

PB_TEST(an_address_in_an_inline_code_span_is_not_linked) {
    auto b = content_blocks("run `curl https://example.org/x` now");
    PB_CHECK(links_of(b[0]).empty());
    PB_CHECK_EQ(plain(b[0]), std::string("run `curl https://example.org/x` now"));
}

PB_TEST(two_links_in_one_line) {
    auto b = content_blocks("[a](https://one.example) and https://two.example/p");
    auto l = links_of(b[0]);
    PB_CHECK_EQ(l.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(l[0].url, std::string("https://one.example"));
    PB_CHECK_EQ(l[1].url, std::string("https://two.example/p"));
}

PB_TEST(links_work_in_headings_and_list_items) {
    auto b = content_blocks("# Title [x](https://h.example)\n- item [y](https://l.example)\n- [ ] task https://t.example");
    PB_CHECK(b[0].kind == BlockKind::Heading);
    PB_CHECK_EQ(links_of(b[0]).size(), static_cast<size_t>(1));
    PB_CHECK(b[1].kind == BlockKind::ListItem);
    PB_CHECK_EQ(links_of(b[1]).size(), static_cast<size_t>(1));
    PB_CHECK(b[2].is_task);
    PB_CHECK_EQ(links_of(b[2]).size(), static_cast<size_t>(1));
}

PB_TEST(an_address_over_2048_bytes_is_not_linked) {
    const std::string long_url = "https://example.org/" + std::string(2049 - 20, 'a');
    auto b = content_blocks("[x](" + long_url + ") and " + long_url);
    PB_CHECK(links_of(b[0]).empty());
}

PB_TEST(adjacent_spans_with_different_targets_are_not_merged) {
    auto b = content_blocks("[a](https://one.example)[b](https://two.example)");
    auto& s = b[0].spans;
    PB_CHECK_EQ(s.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(s[0].url, std::string("https://one.example"));
    PB_CHECK_EQ(s[1].url, std::string("https://two.example"));
}

PB_TEST(line_indexes_are_unchanged_by_links) {
    auto all = parse_markdown("one https://a.example\n\n[t](https://b.example)\n- c");
    PB_CHECK_EQ(all.size(), static_cast<size_t>(4));
    for (size_t i = 0; i < all.size(); ++i) PB_CHECK_EQ(all[i].line_index, static_cast<int>(i));
}

PB_TEST(plain_text_has_no_link_fields) {
    auto b = content_blocks("just text with a.b and example.org without a scheme");
    PB_CHECK(links_of(b[0]).empty());
    for (const auto& sp : b[0].spans) PB_CHECK(!sp.link_confirm);
}

PB_TEST_MAIN()
