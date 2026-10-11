// T048: reading one note out of 1,000 stays far below the 3 s budget (SC-003).
// T079 (spec 002 SC-005): a note with a 200-row table, 50 links and 100 emojis is parsed far below the budget too.
#include <chrono>
#include <string>
#include <vector>

#include "pbnotes/markdown.h"
#include "pbnotes/note_store.h"
#include "test_support.h"
#include "tmp_dir.h"

PB_TEST(open_one_of_a_thousand_notes_is_fast) {
    pbtest::TmpDir d;
    auto store = pbnotes::NoteStore::open(d.file("db"));
    std::string body;
    for (int i = 0; i < 60; ++i)
        body += "- [ ] item " + std::to_string(i) + " with **bold** and *italic* words and a [link](https://example.com)\n";
    std::vector<pbnotes::ApiNote> notes;
    for (int i = 1; i <= 1000; ++i) {
        pbnotes::ApiNote n;
        n.id = i;
        n.etag = "e" + std::to_string(i);
        n.content = "Note " + std::to_string(i) + "\n" + body;  // about 3 KB
        n.modified = 1700000000 + i;
        n.category = i % 7 == 0 ? "Work" : "";
        notes.push_back(n);
    }
    PB_CHECK_EQ(store->apply_remote(notes).inserted, 1000);

    const auto t0 = std::chrono::steady_clock::now();
    auto listing = store->list();
    auto note = store->get_by_remote_id(777);
    auto blocks = pbnotes::parse_markdown(note->content);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

    PB_CHECK_EQ(listing.size(), static_cast<size_t>(1000));
    PB_CHECK(blocks.size() > 60);
    PB_CHECK(ms < 1000);  // the budget is 3 s for the whole screen; the data side must use a fraction of it
}

PB_TEST(a_rich_note_with_a_big_table_many_links_and_emojis_parses_quickly) {
    std::string text = "# Rich note \U0001F600\n\n";
    for (int i = 0; i < 50; ++i)
        text += "Link " + std::to_string(i) + ": [titled](https://example.org/p/" + std::to_string(i) + ") and https://example.org/bare/" +
                std::to_string(i) + "\n";
    for (int i = 0; i < 100; ++i) text += "\U0001F44D\U0001F3FD \U0001F468\u200D\U0001F469\u200D\U0001F467 \U0001F1E9\U0001F1EA ";
    text += "\n\n| Item | Qty | Note |\n|:--|:-:|--:|\n";
    for (int i = 0; i < 200; ++i)
        text += "| item " + std::to_string(i) + " | " + std::to_string(i) + " | **bold** [shop](https://example.org/s) \U0001F600 |\n";
    text += "\n![pic](.attachments.1/a.png)\n[doc](.attachments.1/d.pdf)\n";

    const auto t0 = std::chrono::steady_clock::now();
    auto blocks = pbnotes::parse_markdown(text);
    auto refs = pbnotes::find_attachment_refs(text);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

    int tables = 0, attachments = 0;
    for (const auto& b : blocks) {
        tables += b.kind == pbnotes::BlockKind::Table;
        attachments += b.kind == pbnotes::BlockKind::Attachment;
    }
    PB_CHECK_EQ(tables, 1);
    PB_CHECK_EQ(attachments, 2);
    PB_CHECK_EQ(refs.size(), static_cast<size_t>(2));
    PB_CHECK(ms < 300);   // the budget for opening the note is 3 s (SC-005); parsing may use a tenth of it, in a debug build
}

PB_TEST(hostile_lines_are_parsed_in_linear_time) {
    // many openers and no closers: without a cap every opener would scan to the end of the line
    for (const std::string& unit : {std::string("["), std::string("*a "), std::string("![x]("), std::string("`x "), std::string("| a "), std::string("~~a "), std::string("<u>")}) {
        std::string line;
        for (int i = 0; i < 60000; ++i) line += unit;
        const auto t0 = std::chrono::steady_clock::now();
        auto blocks = pbnotes::parse_markdown(line + "\n" + line + "\n");
        auto refs = pbnotes::find_attachment_refs(line);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        PB_CHECK_EQ(blocks.size(), static_cast<size_t>(2));
        PB_CHECK(refs.empty());
        PB_CHECK(ms < 500);
        // an overlong line is plain text: one span, exactly as written
        PB_CHECK_EQ(blocks[0].spans.size(), static_cast<size_t>(1));
        PB_CHECK_EQ(blocks[0].spans[0].text, line);
    }
}

PB_TEST(hostile_quote_and_details_lines_are_parsed_in_linear_time) {
    // a quote line, a long run of quote markers, and thousands of details lines in one note
    std::string text;
    for (int i = 0; i < 30000; ++i) text += "> ";
    text += "\n";
    for (int i = 0; i < 20000; ++i) text += "<details>\n<summary>s</summary>\n</details>\n> [!NOTE]\n> x\n";
    const auto t0 = std::chrono::steady_clock::now();
    const auto blocks = pbnotes::parse_markdown(text);
    const auto refs = pbnotes::find_attachment_refs(text);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    PB_CHECK(blocks.size() > 100000);
    PB_CHECK(refs.empty());
    PB_CHECK(ms < 2000);
}

PB_TEST_MAIN()
