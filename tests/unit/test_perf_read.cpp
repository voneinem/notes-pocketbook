// T048: reading one note out of 1,000 stays far below the 3 s budget (SC-003).
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

PB_TEST_MAIN()
