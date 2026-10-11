// T078 / spec 004: the conflict-copy title is "<original title> (conflict YYYY-MM-DD)"; repeated conflicts get
// " #2", " #3", ...  Since spec 004 the suffix goes into the stored TITLE; the copy's text is the local text,
// byte for byte.
#include <string>
#include <vector>

#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::conflict_suffix;

namespace {
constexpr int64_t kDay = 1791633600;  // 2026-10-10 12:00:00 UTC

pbnotes::ApiNote server_note(int64_t id, const std::string& content, const std::string& etag, const std::string& title = "") {
    pbnotes::ApiNote n;
    n.id = id;
    n.etag = etag;
    n.content = content;
    n.server_title = title;
    n.modified = 50;
    return n;
}
}  // namespace

PB_TEST(suffix_carries_the_utc_date_and_an_optional_counter) {
    PB_CHECK_EQ(conflict_suffix(kDay), std::string(" (conflict 2026-10-10)"));
    PB_CHECK_EQ(conflict_suffix(kDay, 1), std::string(" (conflict 2026-10-10)"));
    PB_CHECK_EQ(conflict_suffix(kDay, 2), std::string(" (conflict 2026-10-10 #2)"));
    PB_CHECK_EQ(conflict_suffix(kDay, 13), std::string(" (conflict 2026-10-10 #13)"));
}

PB_TEST(the_copy_gets_title_plus_suffix_and_keeps_the_text_untouched) {
    pbtest::TmpDir d;
    auto store = pbnotes::NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Whatever\nserver v1", "s1", "Shopping list")});
    const auto id = store->get_by_remote_id(1)->local_id;
    // a first text line that is not the title, CRLF, trailing blanks: none of it may be touched
    const std::string text = "  First line  \r\n- milk\r\n\r\n";
    store->save_local(id, "Shopping list", text, "", false, 60);
    auto res = store->resolve_conflict(id, server_note(1, "Whatever\nserver v2", "s2", "Shopping list"), kDay);
    PB_CHECK(res.ok && res.created_copy);
    auto copy = store->get(res.copy_local_id);
    PB_CHECK_EQ(copy->title, std::string("Shopping list (conflict 2026-10-10)"));
    PB_CHECK_EQ(copy->content, text);
}

PB_TEST(a_text_that_is_only_the_title_line_is_copied_unchanged) {
    pbtest::TmpDir d;
    auto store = pbnotes::NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "T\nold", "s1", "T")});
    const auto id = store->get_by_remote_id(1)->local_id;
    store->save_local(id, "T", "# T", "", false, 60);
    auto res = store->resolve_conflict(id, server_note(1, "T\nnew", "s2", "T"), kDay);
    PB_CHECK_EQ(store->get(res.copy_local_id)->content, std::string("# T"));
}

PB_TEST(the_copy_is_not_refused_when_the_title_has_the_maximum_length) {
    pbtest::TmpDir d;
    auto store = pbnotes::NoteStore::open(d.file("db"));
    const std::string longest(255, 'x');
    store->apply_remote({server_note(1, "a\nold", "s1", longest)});
    const auto id = store->get_by_remote_id(1)->local_id;
    PB_CHECK(store->save_local(id, longest, "a\nlocal", "", false, 60).error == pbnotes::EditError::None);
    auto res = store->resolve_conflict(id, server_note(1, "a\nnew", "s2", longest), kDay);
    PB_CHECK(res.ok && res.created_copy);
    PB_CHECK_EQ(store->get(res.copy_local_id)->title, longest + " (conflict 2026-10-10)");
}

PB_TEST(repeated_conflicts_on_one_day_are_numbered) {
    pbtest::TmpDir d;
    auto store = pbnotes::NoteStore::open(d.file("db"));
    std::vector<int64_t> copies;
    for (int i = 1; i <= 3; ++i) {
        store->apply_remote({server_note(i, "Same title\nserver version " + std::to_string(i), "s" + std::to_string(i), "Same title")});
        auto id = store->get_by_remote_id(i)->local_id;
        store->save_local(id, "Same title", "Same title\nlocal version " + std::to_string(i), "", false, 60);
        auto res = store->resolve_conflict(id, server_note(i, "Same title\nserver edit " + std::to_string(i), "s" + std::to_string(i) + "b", "Same title"), kDay);
        PB_CHECK(res.ok && res.created_copy);
        copies.push_back(res.copy_local_id);
    }
    PB_CHECK_EQ(store->get(copies[0])->title, std::string("Same title (conflict 2026-10-10)"));
    PB_CHECK_EQ(store->get(copies[1])->title, std::string("Same title (conflict 2026-10-10 #2)"));
    PB_CHECK_EQ(store->get(copies[2])->title, std::string("Same title (conflict 2026-10-10 #3)"));
    PB_CHECK_EQ(store->get(copies[0])->content, std::string("Same title\nlocal version 1"));
}

PB_TEST(identical_text_category_and_favorite_but_a_different_title_is_still_a_conflict) {
    pbtest::TmpDir d;
    auto store = pbnotes::NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Body\ntext", "s1", "Old title")});
    const auto id = store->get_by_remote_id(1)->local_id;
    store->save_local(id, "My new title", "Body\ntext", "", false, 60);   // title-only edit on the device
    // the server renamed the note meanwhile; everything else equal
    auto res = store->resolve_conflict(id, server_note(1, "Body\ntext", "s2", "Their title"), kDay);
    PB_CHECK(res.ok);
    PB_CHECK(res.created_copy);                                    // the device's title is not lost
    PB_CHECK_EQ(store->get(res.copy_local_id)->title, std::string("My new title (conflict 2026-10-10)"));
    PB_CHECK_EQ(store->get(id)->title, std::string("Their title"));
}

PB_TEST(identical_title_text_category_and_favorite_need_no_copy) {
    pbtest::TmpDir d;
    auto store = pbnotes::NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Body\ntext", "s1", "Same")});
    const auto id = store->get_by_remote_id(1)->local_id;
    store->save_local(id, "Same", "Body\ntext", "Work", false, 60);
    pbnotes::ApiNote same = server_note(1, "Body\ntext", "s2", "Same");
    same.category = "Work";
    auto res = store->resolve_conflict(id, same, kDay);
    PB_CHECK(res.ok);
    PB_CHECK(!res.created_copy);
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(1));
}

PB_TEST_MAIN()
