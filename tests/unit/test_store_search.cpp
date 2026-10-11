// T086: case-insensitive substring search over title and content, including non-ASCII letters;
// combinable with views and sorting; 1,000 notes searched within the 2 s budget (SC-008).
#include <chrono>
#include <string>
#include <vector>

#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::ListQuery;
using pbnotes::NoteStore;

namespace {
ListQuery search(const std::string& text, const std::string& view = "all", const std::string& sort = "modified") {
    return ListQuery{view, sort, text};
}
std::vector<std::string> titles(const std::vector<pbnotes::NoteSummary>& v) {
    std::vector<std::string> t;
    for (const auto& s : v) t.push_back(s.title);
    return t;
}
}  // namespace

PB_TEST(matches_title_and_content_ignoring_case) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->create_local(pbnotes::derive_title("Shopping list\nmilk and bread"), "Shopping list\nmilk and bread", "", false, 1);
    store->create_local(pbnotes::derive_title("Meeting\nTalk about the BUDGET"), "Meeting\nTalk about the BUDGET", "", false, 2);
    store->create_local(pbnotes::derive_title("Ideas\nnothing relevant"), "Ideas\nnothing relevant", "", false, 3);
    PB_CHECK(titles(store->list(search("shopping"))) == (std::vector<std::string>{"Shopping list"}));
    PB_CHECK(titles(store->list(search("budget"))) == (std::vector<std::string>{"Meeting"}));   // only in the body
    PB_CHECK(titles(store->list(search("BREAD"))) == (std::vector<std::string>{"Shopping list"}));
    PB_CHECK(store->list(search("zebra")).empty());
}

PB_TEST(substrings_match_inside_words) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->create_local(pbnotes::derive_title("Banana bread\nrecipe"), "Banana bread\nrecipe", "", false, 1);
    PB_CHECK_EQ(store->list(search("nan")).size(), static_cast<size_t>(1));
    PB_CHECK_EQ(store->list(search("ana b")).size(), static_cast<size_t>(1));  // across a space
}

PB_TEST(non_ascii_letters_fold_case) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->create_local(pbnotes::derive_title("Äpfel und Birnen\nÖl"), "Äpfel und Birnen\nÖl", "", false, 1);
    store->create_local(pbnotes::derive_title("ΑΘΗΝΑ\nΕλλάδα"), "ΑΘΗΝΑ\nΕλλάδα", "", false, 2);
    store->create_local(pbnotes::derive_title("МОСКВА\nстолица"), "МОСКВА\nстолица", "", false, 3);
    PB_CHECK_EQ(store->list(search("äpfel")).size(), static_cast<size_t>(1));
    PB_CHECK_EQ(store->list(search("ÄPFEL")).size(), static_cast<size_t>(1));
    PB_CHECK_EQ(store->list(search("öl")).size(), static_cast<size_t>(1));
    PB_CHECK_EQ(store->list(search("αθηνα")).size(), static_cast<size_t>(1));
    PB_CHECK_EQ(store->list(search("москва")).size(), static_cast<size_t>(1));
    PB_CHECK_EQ(store->list(search("СТОЛИЦА")).size(), static_cast<size_t>(1));
}

PB_TEST(special_characters_are_literal) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->create_local(pbnotes::derive_title("Sale\n100% off_today"), "Sale\n100% off_today", "", false, 1);
    store->create_local(pbnotes::derive_title("Other\nplain text"), "Other\nplain text", "", false, 2);
    PB_CHECK_EQ(store->list(search("100%")).size(), static_cast<size_t>(1));
    PB_CHECK_EQ(store->list(search("%")).size(), static_cast<size_t>(1));   // not a wildcard
    PB_CHECK_EQ(store->list(search("_")).size(), static_cast<size_t>(1));
    PB_CHECK(store->list(search("'; DROP TABLE notes; --")).empty());
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(2));
}

PB_TEST(empty_or_blank_search_means_no_filter) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->create_local(pbnotes::derive_title("A\nx"), "A\nx", "", false, 1);
    store->create_local(pbnotes::derive_title("B\ny"), "B\ny", "", false, 2);
    PB_CHECK_EQ(store->list(search("")).size(), static_cast<size_t>(2));
    PB_CHECK_EQ(store->list(search("   ")).size(), static_cast<size_t>(2));
}

PB_TEST(search_combines_with_views_and_sorting) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->create_local(pbnotes::derive_title("Zeta recipe\nsoup"), "Zeta recipe\nsoup", "Food", true, 1);
    store->create_local(pbnotes::derive_title("Alpha recipe\nsalad"), "Alpha recipe\nsalad", "Food", false, 2);
    store->create_local(pbnotes::derive_title("Beta recipe\ncake"), "Beta recipe\ncake", "Other", true, 3);
    store->create_local(pbnotes::derive_title("Gamma\nno match"), "Gamma\nno match", "Food", true, 4);
    PB_CHECK(titles(store->list(search("recipe", "category:Food", "title"))) ==
             (std::vector<std::string>{"Alpha recipe", "Zeta recipe"}));
    PB_CHECK(titles(store->list(search("recipe", "favorites", "modified"))) ==
             (std::vector<std::string>{"Beta recipe", "Zeta recipe"}));
    PB_CHECK(titles(store->list(search("recipe", "uncategorized"))).empty());
}

PB_TEST(deleted_notes_are_never_found) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    pbnotes::ApiNote n;
    n.id = 1;
    n.etag = "e";
    n.content = "Secret plan\nbody";
    n.modified = 5;
    store->apply_remote({n});
    store->mark_deleted(store->get_by_remote_id(1)->local_id);
    PB_CHECK(store->list(search("secret")).empty());
}

PB_TEST(searching_a_thousand_notes_is_fast) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    std::string body;
    for (int i = 0; i < 60; ++i) body += "line " + std::to_string(i) + " with ordinary words and some more text\n";
    std::vector<pbnotes::ApiNote> notes;
    for (int i = 1; i <= 1000; ++i) {
        pbnotes::ApiNote n;
        n.id = i;
        n.etag = "e" + std::to_string(i);
        n.content = "Note " + std::to_string(i) + "\n" + body + (i == 777 ? "the needle is here\n" : "");
        n.modified = 1700000000 + i;
        notes.push_back(n);
    }
    store->apply_remote(notes);
    const auto t0 = std::chrono::steady_clock::now();
    auto hit = store->list(search("NEEDLE"));
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    PB_CHECK_EQ(hit.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(hit[0].title, std::string("Note 777"));
    PB_CHECK(ms < 1000);  // the budget is 2 s for the screen (SC-008)
}

PB_TEST_MAIN()
