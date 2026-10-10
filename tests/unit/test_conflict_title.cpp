// T078: the conflict-copy title "<original title> (conflict YYYY-MM-DD)" is written into the first
// line of the copy's content; repeated conflicts get " #2", " #3", ...
#include <string>

#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::conflict_suffix;
using pbnotes::derive_title;
using pbnotes::with_title_suffix;

namespace {
constexpr int64_t kDay = 1791633600;  // 2026-10-10 12:00:00 UTC
}

PB_TEST(suffix_carries_the_utc_date_and_an_optional_counter) {
    PB_CHECK_EQ(conflict_suffix(kDay), std::string(" (conflict 2026-10-10)"));
    PB_CHECK_EQ(conflict_suffix(kDay, 1), std::string(" (conflict 2026-10-10)"));
    PB_CHECK_EQ(conflict_suffix(kDay, 2), std::string(" (conflict 2026-10-10 #2)"));
    PB_CHECK_EQ(conflict_suffix(kDay, 13), std::string(" (conflict 2026-10-10 #13)"));
}

PB_TEST(the_suffix_goes_into_the_first_non_empty_line) {
    const std::string s = conflict_suffix(kDay);
    PB_CHECK_EQ(with_title_suffix("Meeting\nbody\n", s), std::string("Meeting (conflict 2026-10-10)\nbody\n"));
    PB_CHECK_EQ(with_title_suffix("\n\n  Meeting  \nbody", s), std::string("\n\n  Meeting (conflict 2026-10-10)\nbody"));
    PB_CHECK_EQ(with_title_suffix("Title\r\nbody\r\n", s), std::string("Title (conflict 2026-10-10)\r\nbody\r\n"));
    PB_CHECK_EQ(with_title_suffix("# Heading", s), std::string("# Heading (conflict 2026-10-10)"));
    PB_CHECK_EQ(with_title_suffix("Größe 日本語", s), std::string("Größe 日本語 (conflict 2026-10-10)"));
}

PB_TEST(the_derived_title_of_the_copy_is_title_plus_suffix) {
    const std::string s = conflict_suffix(kDay, 2);
    const std::string content = "Shopping list\n- milk\n";
    PB_CHECK_EQ(derive_title(with_title_suffix(content, s)), derive_title(content) + s);
}

PB_TEST(a_whitespace_only_text_gets_no_marker) {
    PB_CHECK_EQ(with_title_suffix("  \n ", conflict_suffix(kDay)), std::string("  \n "));
}

PB_TEST(repeated_conflicts_on_one_day_are_numbered) {
    pbtest::TmpDir d;
    auto store = pbnotes::NoteStore::open(d.file("db"));
    std::vector<int64_t> copies;
    for (int i = 1; i <= 3; ++i) {
        pbnotes::ApiNote server;
        server.id = i;
        server.etag = "s" + std::to_string(i);
        server.content = "Same title\nserver version " + std::to_string(i);
        server.modified = 50;
        store->apply_remote({server});
        auto id = store->get_by_remote_id(i)->local_id;
        store->save_local(id, "Same title\nlocal version " + std::to_string(i), "", false, 60);
        pbnotes::ApiNote newer = server;
        newer.etag = "s" + std::to_string(i) + "b";
        newer.content = "Same title\nserver edit " + std::to_string(i);
        auto res = store->resolve_conflict(id, newer, kDay);
        PB_CHECK(res.ok && res.created_copy);
        copies.push_back(res.copy_local_id);
    }
    PB_CHECK_EQ(store->get(copies[0])->title, std::string("Same title (conflict 2026-10-10)"));
    PB_CHECK_EQ(store->get(copies[1])->title, std::string("Same title (conflict 2026-10-10 #2)"));
    PB_CHECK_EQ(store->get(copies[2])->title, std::string("Same title (conflict 2026-10-10 #3)"));
    PB_CHECK(store->get(copies[0])->content.find("(conflict 2026-10-10)\nlocal version 1") != std::string::npos);
}

PB_TEST_MAIN()
