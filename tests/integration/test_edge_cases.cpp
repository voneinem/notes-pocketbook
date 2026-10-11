// T096: edge cases from the spec — very long notes, duplicate titles, a wrong device clock,
// categories with "/", Unicode — none of them loses or merges data.
#include <string>

#include "fake_notes_server.h"
#include "fake_transport.h"
#include "pbnotes/markdown.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "pbnotes/sync_engine.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::NoteStatus;
using pbnotes::SyncEngine;
using pbnotes::SyncOutcome;

namespace {
struct Env {
    pbtest::TmpDir dir;
    std::unique_ptr<pbnotes::NoteStore> store;
    pbtest::FakeNotesServer server;
    pbtest::FakeTransport transport;
    Env() {
        store = pbnotes::NoteStore::open(dir.file("db"));
        pbnotes::Account a;
        a.server_url = "https://cloud.example.com";
        a.username = "alice";
        a.password = "pw";
        store->save_account(a);
        transport.set_handler([this](const pbnotes::Request& r) { return server.handle(r); });
    }
    pbnotes::SyncResult run() {
        SyncEngine engine(*store, transport, [] { return int64_t(1700001000); });
        return engine.run();
    }
};
}  // namespace

PB_TEST(a_very_long_note_is_saved_parsed_and_synced) {
    Env e;
    std::string text = "Long note\n";
    for (int i = 0; i < 2000; ++i) text += "- [ ] task number " + std::to_string(i) + " with **bold** words and a few more\n";
    PB_CHECK(text.size() > 100000);
    auto r = e.store->create_local(pbnotes::derive_title(text), text, "Work", false, 1000);
    PB_CHECK(r.error == pbnotes::EditError::None);
    PB_CHECK_EQ(e.store->get(r.local_id)->content, text);
    PB_CHECK_EQ(pbnotes::parse_markdown(text).size(), static_cast<size_t>(2001));
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.count_with_content(text), 1);
    // toggling one of the thousands of checkboxes still changes exactly one byte
    auto toggled = e.store->toggle_task(r.local_id, 1500, 2000);
    PB_CHECK(toggled.error == pbnotes::EditError::None);
    PB_CHECK_EQ(e.store->get(r.local_id)->content.size(), text.size());
}

PB_TEST(notes_with_the_same_title_stay_distinct_through_a_sync) {
    Env e;
    auto a = e.store->create_local(pbnotes::derive_title("Shopping\nmilk"), "Shopping\nmilk", "", false, 1000).local_id;
    auto b = e.store->create_local(pbnotes::derive_title("Shopping\neggs"), "Shopping\neggs", "", false, 1001).local_id;
    PB_CHECK(a != b);
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.notes.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(2));
    PB_CHECK_EQ(e.store->get(a)->content, std::string("Shopping\nmilk"));
    PB_CHECK_EQ(e.store->get(b)->content, std::string("Shopping\neggs"));
    // a later pull does not merge them either
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(2));
}

PB_TEST(a_wrong_device_clock_loses_nothing) {
    for (int64_t wrong_now : {int64_t(5), int64_t(4102444800LL)}) {  // 1970 and 2100
        Env e;
        e.server.add_on_server("Server note\nbody");
        e.run();
        auto id = e.store->get_by_remote_id(1)->local_id;
        e.store->save_local(id, pbnotes::derive_title("Server note\nedited with a wrong clock"), "Server note\nedited with a wrong clock", "", false, wrong_now);
        auto fresh = e.store->create_local(pbnotes::derive_title("Fresh\nwritten with a wrong clock"), "Fresh\nwritten with a wrong clock", "", false, wrong_now).local_id;
        auto r = e.run();
        PB_CHECK(r.outcome == SyncOutcome::Success);
        PB_CHECK_EQ(e.server.notes.at(1).content, std::string("Server note\nedited with a wrong clock"));
        PB_CHECK_EQ(e.server.count_with_content("Fresh\nwritten with a wrong clock"), 1);
        PB_CHECK(e.store->get(id)->status == NoteStatus::Void);
        PB_CHECK(e.store->get(fresh)->status == NoteStatus::Void);
        PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(2));
    }
}

PB_TEST(categories_with_slashes_round_trip_and_invalid_ones_are_rejected) {
    Env e;
    PB_CHECK(e.store->create_local(pbnotes::derive_title("A\nx"), "A\nx", "Work/Projects/2026", false, 1).error == pbnotes::EditError::None);
    PB_CHECK(e.store->create_local(pbnotes::derive_title("B\nx"), "B\nx", "", false, 2).error == pbnotes::EditError::None);                // uncategorized
    PB_CHECK(e.store->create_local(pbnotes::derive_title("C\nx"), "C\nx", std::string(256, 'x'), false, 3).error == pbnotes::EditError::InvalidCategory);
    e.run();
    PB_CHECK_EQ(e.server.notes.size(), static_cast<size_t>(2));
    PB_CHECK_EQ(e.server.notes.at(1).category, std::string("Work/Projects/2026"));
    PB_CHECK_EQ(e.server.notes.at(2).category, std::string(""));
    pbnotes::ListQuery q;
    q.view = "category:Work/Projects/2026";
    PB_CHECK_EQ(e.store->list(q).size(), static_cast<size_t>(1));
}

PB_TEST(unicode_and_special_characters_survive_the_round_trip) {
    Env e;
    const std::string text = "Größe 日本語 🙂 \"quotes\" back\\slash <tag> & tab\there\nline two";
    const std::string title = "Größe 日本語 🙂 \"quotes\" back\\slash <tag> &";   // titles may not hold a tab (control character)
    auto id = e.store->create_local(title, text, "Ünï/çödé", true, 1000).local_id;
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.notes.at(1).content, text);
    PB_CHECK_EQ(e.server.notes.at(1).title, title);
    // pull it into a second, empty device
    Env other;
    other.server.notes = e.server.notes;
    other.server.next_id = e.server.next_id;
    other.run();
    auto n = other.store->get_by_remote_id(1);
    PB_CHECK(n.has_value());
    PB_CHECK_EQ(n->content, text);
    PB_CHECK_EQ(n->title, title);
    PB_CHECK_EQ(n->category, std::string("Ünï/çödé"));
    PB_CHECK(n->favorite);
    (void)id;
}

PB_TEST_MAIN()
