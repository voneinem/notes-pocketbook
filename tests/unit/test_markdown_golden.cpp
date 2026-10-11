// Spec 006 T002: a characterization of the Markdown parser for notes that use none of the formats added by that
// feature (no fence, quote, <details>, ~~ or <u>). The dump of every block of a fixed corpus is compared with
// tests/fixtures/markdown_golden.txt, which was generated with the parser as it was before feature 006. It guards
// FR-020: nothing outside the new formats may change. Rewrite the file with PBNOTES_UPDATE_GOLDEN=1 (only when a
// change of the old behavior is intended and explained).
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "pbnotes/markdown.h"
#include "test_support.h"

using pbnotes::Block;
using pbnotes::BlockKind;
using pbnotes::Span;

namespace {

std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c == '\\') o += "\\\\";
        else o += c;
    }
    return o;
}

const char* kind_name(BlockKind k) {
    switch (k) {
        case BlockKind::Heading: return "Heading";
        case BlockKind::Paragraph: return "Paragraph";
        case BlockKind::ListItem: return "ListItem";
        case BlockKind::Blank: return "Blank";
        case BlockKind::Table: return "Table";
        case BlockKind::Attachment: return "Attachment";
        default: return "Other";
    }
}

std::string spans_dump(const std::vector<Span>& spans) {
    std::ostringstream o;
    for (const Span& s : spans) {
        o << " [" << static_cast<int>(s.kind) << " '" << esc(s.text) << "'";
        if (!s.url.empty()) o << " url=" << s.url << " confirm=" << s.link_confirm;
        o << "]";
    }
    return o.str();
}

std::string dump(const std::string& text) {
    std::ostringstream o;
    for (const Block& b : pbnotes::parse_markdown(text)) {
        o << kind_name(b.kind) << " line=" << b.line_index << " count=" << b.line_count << " level=" << b.level
          << " ordered=" << b.ordered << " number=" << b.number << " depth=" << b.depth << " task=" << b.is_task
          << " checked=" << b.checked << " src=" << b.src_begin << ".." << b.src_end;
        if (b.kind == BlockKind::Table) {
            o << " cols=" << b.table.align.size() << " rows=" << b.table.rows.size();
            for (auto a : b.table.align) o << " a" << static_cast<int>(a);
            for (const auto& row : b.table.rows) {
                o << "\n  row";
                for (const auto& cell : row) o << " |" << spans_dump(cell);
            }
        } else if (b.kind == BlockKind::Attachment) {
            o << " name='" << esc(b.attachment.name) << "' path='" << esc(b.attachment.path)
              << "' token=" << b.attachment.token_id << " image=" << b.attachment.is_image_syntax;
        } else {
            o << spans_dump(b.spans);
        }
        o << "\n";
    }
    return o.str();
}

std::vector<std::string> corpus() {
    return {
        "",
        "plain text",
        "line one\nline two\n\nline four",
        "# H1\n## H2\n### H3\n#### H4\n##### H5\n###### H6",
        "####### seven\n#nospace\n#\n#\ttab\n    # indented",
        "# Heading with **bold** and *italic*",
        "plain **bold** and *italic* and ***both*** end",
        "unmatched ** here\n**open\nclose**\n** spaced **",
        "_under_ and __double__ and snake_case_word",
        "escaped \\*star\\* and \\_under\\_ and \\[brackets\\]",
        "- one\n- two\n  - nested\n    - deeper\n* star\n+ plus",
        "1. first\n2. second\n10) tenth\n   3. indented",
        "- [ ] open task\n- [x] done task\n- [X] DONE\n  - [ ] nested task\n- [ ]no space\n- [] empty",
        "[title](https://example.org/a) and https://example.org/b, and <https://example.org/c>.",
        "[mailto](mailto:a@b.example) [file](file:///etc/passwd) [rel](./x.md)",
        "text [label that differs](https://example.org/other) and (https://example.org/paren)",
        "![alt](https://example.org/i.png) and ![](https://example.org/j.png)",
        "[doc](.attachments.12/file%20name.pdf)\n![pic](.attachments.12/pic.png)\n![local](pbatt:42)",
        "before ![pic](.attachments.1/a.png) after ![two](.attachments.1/b.png) end",
        "- [ ] ![pic](.attachments.1/a.png) task with image",
        "| a | b |\n|---|---|\n| 1 | 2 |",
        "| left | mid | right |\n|:--|:-:|--:|\n| a | b | c |\n| longer cell | **bold** | [l](https://example.org) |",
        "| a | b |\n| 1 | 2 |",
        "| a |\n|---|\n| only |\nafter table",
        "x\n| a | b |\n|---|---|\n| 1 |\n| 1 | 2 | 3 |\n\ntext",
        "before\n\n\n\nafter\n   \nend",
        "crlf line one\r\ncrlf **two**\r\n- item\r\n",
        std::string(20000, 'x') + "\n**not bold** after a very long line",
        "**" + std::string(17000, 'y') + "**",
        "emoji 😀 and flags 🇩🇪 and e\xCC\x81 combining **ü** bold",
        "`code` with **bold** and `*not italic*` and a `` double ``",
        "\\# not a heading\n\\- not a list\n1\\. not numbered",
        "tab\tseparated\t**bold**\n\t- tab-indented item",
        "a | b without table\n--- | ---",
        "Title\n=====\nsetext is not supported\n-----",
        "<b>raw html</b> & ampersand < less > greater \"quote\"",
        "[nested [brackets]](https://example.org) and [unclosed(https://example.org)",
        "- item with [link](https://example.org) and **bold [inner](https://example.org) text**",
        "https://example.org/path?query=1&b=2#frag and (https://example.org/x) and \"https://example.org/y\".",
        "line with trailing spaces   \nnext",
        "1. one\n\n2. two after blank\n\n- bullet after blank",
    };
}

}  // namespace

PB_TEST(parser_output_for_old_syntax_is_unchanged) {
    std::ostringstream all;
    int n = 0;
    for (const std::string& note : corpus()) {
        all << "=== " << ++n << "\n" << dump(note);
    }
    const std::string actual = all.str();
    const std::string path = std::string(PBNOTES_SOURCE_DIR) + "/tests/fixtures/markdown_golden.txt";
    if (std::getenv("PBNOTES_UPDATE_GOLDEN") != nullptr) {
        std::ofstream out(path, std::ios::binary);
        out << actual;
        return;
    }
    std::ifstream in(path, std::ios::binary);
    PB_CHECK(in.good());
    const std::string golden((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (golden != actual) {
        // find the first difference to keep the failure readable
        size_t i = 0;
        while (i < golden.size() && i < actual.size() && golden[i] == actual[i]) ++i;
        const size_t from = i > 80 ? i - 80 : 0;
        PB_FAIL_AT("parser output differs from the golden file near byte " + std::to_string(i) + "\n golden: " +
                   esc(golden.substr(from, 200)) + "\n actual: " + esc(actual.substr(from, 200)));
    }
}

PB_TEST_MAIN()
