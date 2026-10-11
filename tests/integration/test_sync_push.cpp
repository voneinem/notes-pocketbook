// T068: SyncEngine push — order, per-note behavior, and what a failing note may (not) affect.
#include <string>

#include "fake_notes_server.h"
#include "fake_transport.h"
#include "pbnotes/json.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "pbnotes/sync_engine.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::ErrorClass;
using pbnotes::NoteStatus;
using pbnotes::NoteStore;
using pbnotes::SyncEngine;
using pbnotes::SyncOutcome;

namespace {

struct Env {
    pbtest::TmpDir dir;
    std::unique_ptr<NoteStore> store;
    pbtest::FakeNotesServer server;
    pbtest::FakeTransport transport;
    int64_t now = 1700001000;

    Env() {
        store = NoteStore::open(dir.file("pbnotes.db"));
        pbnotes::Account a;
        a.server_url = "https://cloud.example.com";
        a.username = "alice";
        a.password = "app-password";
        store->save_account(a);
        transport.set_handler([this](const pbnotes::Request& r) { return server.handle(r); });
        server.add_on_server("First note\nbody one", "Home");
        server.add_on_server("Second note\nbody two");
        server.add_on_server("Third note\nbody three", "Work", true);
    }
    pbnotes::SyncResult run() {
        SyncEngine engine(*store, transport, [this] { return now; });
        return engine.run();
    }
    int64_t local_of(int64_t remote_id) { return store->get_by_remote_id(remote_id)->local_id; }
    // Index of the first request in server.log that starts with `prefix` (-1 if none).
    int index_of(const std::string& prefix) const {
        for (size_t i = 0; i < server.log.size(); ++i)
            if (server.log[i].compare(0, prefix.size(), prefix) == 0) return static_cast<int>(i);
        return -1;
    }
    int last_index_of(const std::string& prefix) const {
        int idx = -1;
        for (size_t i = 0; i < server.log.size(); ++i)
            if (server.log[i].compare(0, prefix.size(), prefix) == 0) idx = static_cast<int>(i);
        return idx;
    }
};

}  // namespace

PB_TEST(local_changes_are_pushed_before_the_remote_changes_are_pulled) {
    Env e;
    e.run();  // baseline: three notes, all void
    e.store->create_local(pbnotes::derive_title("Brand new\nfrom the device"), "Brand new\nfrom the device", "Work", false, 2000);
    e.store->save_local(e.local_of(1), pbnotes::derive_title("First note\nedited on device"), "First note\nedited on device", "Home", false, 2100);
    e.store->mark_deleted(e.local_of(2));
    e.server.log.clear();

    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    const int list = e.index_of("GET /notes?") >= 0 ? e.index_of("GET /notes?") : e.index_of("GET /notes");
    // every write (and the delete check) came before the first listing request
    PB_CHECK(e.last_index_of("POST /notes") < list);
    PB_CHECK(e.last_index_of("PUT /notes/") < list);
    PB_CHECK(e.last_index_of("DELETE /notes/") < list);
    PB_CHECK(e.last_index_of("GET /notes/2") < list);
    PB_CHECK(list > 0);
}

PB_TEST(a_new_note_is_posted_and_gets_its_server_identity) {
    Env e;
    e.run();
    auto id = e.store->create_local(pbnotes::derive_title("Brand new\nfrom the device"), "Brand new\nfrom the device", "Work", true, 2000).local_id;
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.pushed, 1);
    auto n = e.store->get(id);
    PB_CHECK(n->status == NoteStatus::Void);
    PB_CHECK(n->remote_id.has_value());
    PB_CHECK(n->etag.has_value());
    PB_CHECK_EQ(n->title, std::string("Brand new"));
    PB_CHECK_EQ(e.server.count_with_content("Brand new\nfrom the device"), 1);
    PB_CHECK(e.server.notes.at(*n->remote_id).favorite);
    PB_CHECK_EQ(e.server.notes.at(*n->remote_id).category, std::string("Work"));
}

PB_TEST(an_edited_note_is_put_with_the_stored_etag_and_confirmed) {
    Env e;
    e.run();
    auto id = e.local_of(1);
    const std::string old_etag = *e.store->get(id)->etag;
    e.store->save_local(id, pbnotes::derive_title("First note\nedited on device"), "First note\nedited on device", "Home", false, 2100);
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.pushed, 1);
    bool saw_put = false;
    for (const auto& req : e.transport.requests()) {
        if (req.method != "PUT") continue;
        saw_put = true;
        PB_CHECK_EQ(pbnotes::header_value(req.headers, "If-Match"), "\"" + old_etag + "\"");
    }
    PB_CHECK(saw_put);
    auto n = e.store->get(id);
    PB_CHECK(n->status == NoteStatus::Void);
    PB_CHECK(*n->etag != old_etag);  // the server's new validator
    PB_CHECK_EQ(e.server.notes.at(1).content, std::string("First note\nedited on device"));
}

PB_TEST(a_deleted_note_is_checked_then_deleted_and_the_row_removed) {
    Env e;
    e.run();
    auto id = e.local_of(2);
    e.store->mark_deleted(id);
    e.server.log.clear();
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK(e.index_of("GET /notes/2") >= 0);
    PB_CHECK(e.index_of("GET /notes/2") < e.index_of("DELETE /notes/2"));
    PB_CHECK(!e.store->get(id).has_value());
    PB_CHECK(e.server.notes.count(2) == 0);
}

PB_TEST(a_note_deleted_locally_but_changed_on_the_server_is_not_deleted) {
    Env e;
    e.run();
    auto id = e.local_of(2);
    e.store->mark_deleted(id);
    e.server.edit_on_server(2, "Second note\nsomeone else edited this");
    e.server.log.clear();
    auto r = e.run();
    PB_CHECK_EQ(e.server.requests_matching("DELETE"), 0);                  // never deleted
    PB_CHECK(e.server.notes.count(2) == 1);
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.restored, 1);                                     // the server's version comes back
    PB_CHECK(e.store->get(id).has_value());
    PB_CHECK(e.store->get(id)->status == NoteStatus::Void);
    PB_CHECK_EQ(e.store->get(id)->content, std::string("Second note\nsomeone else edited this"));
}

PB_TEST(a_note_that_is_already_gone_on_the_server_is_just_removed_locally) {
    Env e;
    e.run();
    auto id = e.local_of(2);
    e.store->mark_deleted(id);
    e.server.notes.erase(2);
    e.server.log.clear();
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.requests_matching("DELETE"), 0);
    PB_CHECK(!e.store->get(id).has_value());
}

PB_TEST(a_failing_note_does_not_stop_the_others) {
    Env e;
    e.run();
    auto id1 = e.local_of(1);
    auto id3 = e.local_of(3);
    e.store->save_local(id1, pbnotes::derive_title("First note\nedit A"), "First note\nedit A", "Home", false, 2100);
    e.store->save_local(id3, pbnotes::derive_title("Third note\nedit B"), "Third note\nedit B", "Work", true, 2200);
    e.transport.set_handler([&](const pbnotes::Request& req) {
        if (req.method == "PUT" && req.url.find("/notes/1") != std::string::npos) return pbtest::FakeTransport::status(500);
        return e.server.handle(req);
    });
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Partial);
    PB_CHECK(e.store->get(id1)->status == NoteStatus::Edited);  // stays pending
    PB_CHECK_EQ(e.store->get(id1)->content, std::string("First note\nedit A"));
    PB_CHECK(e.store->get(id3)->status == NoteStatus::Void);    // the other one went through
    PB_CHECK_EQ(e.server.notes.at(3).content, std::string("Third note\nedit B"));
}

PB_TEST(a_stale_put_is_never_overwritten_and_keeps_the_local_text) {
    Env e;
    e.run();
    auto id = e.local_of(1);
    e.store->save_local(id, pbnotes::derive_title("First note\nmy local edit"), "First note\nmy local edit", "Home", false, 2100);
    e.server.edit_on_server(1, "First note\nedited elsewhere");
    e.server.log.clear();
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.conflicts, 1);
    // the server's note was not overwritten ...
    PB_CHECK_EQ(e.server.notes.at(1).content, std::string("First note\nedited elsewhere"));
    PB_CHECK_EQ(e.store->get(id)->content, std::string("First note\nedited elsewhere"));
    // ... and the local text was not lost: it lives on as a conflict copy
    bool found = false;
    for (const auto& s : e.store->list()) {
        auto n = e.store->get(s.local_id);
        if (n->content.find("my local edit") != std::string::npos) found = true;
    }
    PB_CHECK(found);
}

PB_TEST(a_note_deleted_on_the_server_is_recreated_from_the_local_edit) {
    Env e;
    e.run();
    auto id = e.local_of(1);
    e.store->save_local(id, pbnotes::derive_title("First note\nedited after remote delete"), "First note\nedited after remote delete", "Home", false, 2100);
    e.server.notes.erase(1);
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.recreated, 1);
    auto n = e.store->get(id);
    PB_CHECK(n->status == NoteStatus::Void);
    PB_CHECK(*n->remote_id != 1);
    PB_CHECK_EQ(e.server.notes.at(*n->remote_id).content, std::string("First note\nedited after remote delete"));
}

PB_TEST(without_pending_changes_only_the_listing_is_requested) {
    Env e;
    e.run();
    e.server.log.clear();
    e.run();
    PB_CHECK_EQ(e.server.log.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(e.server.log[0].compare(0, 10, "GET /notes"), 0);
}

PB_TEST(a_network_failure_during_the_push_stops_the_run_and_keeps_everything_pending) {
    Env e;
    e.run();
    auto id1 = e.local_of(1);
    auto id2 = e.local_of(2);
    e.store->save_local(id1, pbnotes::derive_title("First note\nedit A"), "First note\nedit A", "Home", false, 2100);
    e.store->save_local(id2, pbnotes::derive_title("Second note\nedit B"), "Second note\nedit B", "", false, 2200);
    e.transport.fail_request(e.transport.requests().size(), ErrorClass::Network);  // the first write
    e.server.log.clear();
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Offline);
    PB_CHECK(e.server.log.empty());                    // the run stopped, no further requests
    PB_CHECK(e.store->get(id1)->status == NoteStatus::Edited);
    PB_CHECK(e.store->get(id2)->status == NoteStatus::Edited);
}

PB_TEST(rejected_credentials_stop_the_run_without_touching_local_data) {
    Env e;
    e.run();
    auto id = e.local_of(1);
    e.store->save_local(id, pbnotes::derive_title("First note\nedit"), "First note\nedit", "Home", false, 2100);
    e.transport.set_handler([](const pbnotes::Request&) { return pbtest::FakeTransport::status(401); });
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Failed);
    PB_CHECK(r.error_class == ErrorClass::Auth);
    PB_CHECK(e.store->get(id)->status == NoteStatus::Edited);
    PB_CHECK_EQ(e.store->get(id)->content, std::string("First note\nedit"));
}

// ---- spec 004: the explicit title travels with the note --------------------------------------------------------------

PB_TEST(a_new_note_is_posted_with_the_title_the_user_entered) {
    Env e;
    e.run();
    auto id = e.store->create_local("Groceries", "Buy milk\nand eggs", "", false, 2000).local_id;
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    const auto remote = *e.store->get(id)->remote_id;
    PB_CHECK_EQ(e.server.notes.at(remote).title, std::string("Groceries"));       // not "Buy milk"
    PB_CHECK_EQ(e.store->get(id)->title, std::string("Groceries"));
    PB_CHECK_EQ(e.store->get(id)->content, std::string("Buy milk\nand eggs"));
}

PB_TEST(every_update_sends_the_title_even_when_only_the_category_changed) {
    Env e;
    e.run();
    const auto id = e.local_of(1);
    const std::string title = e.store->get(id)->title;
    e.store->set_category(id, "Elsewhere", 2100);
    e.server.write_bodies.clear();
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.write_bodies.size(), static_cast<size_t>(1));
    auto body = pbnotes::json::parse(e.server.write_bodies[0]);
    PB_CHECK_EQ(body->get_string("title").value_or("?"), title);
    PB_CHECK_EQ(e.server.notes.at(1).title, title);
}

PB_TEST(a_title_only_edit_is_pushed_and_confirmed) {
    Env e;
    e.run();
    const auto id = e.local_of(1);
    e.store->save_local(id, "A better title", "First note\nbody one", "Home", false, 2100);
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.pushed, 1);
    PB_CHECK_EQ(e.server.notes.at(1).title, std::string("A better title"));
    PB_CHECK_EQ(e.server.notes.at(1).content, std::string("First note\nbody one"));
    PB_CHECK(e.store->get(id)->status == NoteStatus::Void);
    PB_CHECK_EQ(e.store->get(id)->title, std::string("A better title"));
}

PB_TEST(a_title_edited_offline_reaches_the_server_on_the_next_sync) {
    Env e;
    e.run();
    const auto id = e.local_of(2);
    e.store->save_local(id, "Offline title", "Second note\nbody two", "", false, 2100);
    e.transport.fail_request(e.transport.requests().size(), ErrorClass::Network);   // the push fails first
    PB_CHECK(e.run().outcome == SyncOutcome::Offline);
    PB_CHECK(e.store->get(id)->status == NoteStatus::Edited);
    PB_CHECK_EQ(e.store->get(id)->title, std::string("Offline title"));
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.notes.at(2).title, std::string("Offline title"));
    PB_CHECK(e.store->get(id)->status == NoteStatus::Void);
}

PB_TEST(a_title_the_server_adjusted_is_what_the_note_shows_afterwards) {
    Env e;
    e.run();
    e.server.adjust_title = [](const std::string& t) { return t + " (2)"; };
    auto id = e.store->create_local("Same title", "text", "", false, 2000).local_id;
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.store->get(id)->title, std::string("Same title (2)"));
    PB_CHECK(e.store->get(id)->status == NoteStatus::Void);
}

PB_TEST(a_title_changed_while_the_request_was_in_flight_keeps_the_note_pending) {
    Env e;
    e.run();
    const auto id = e.local_of(1);
    e.store->save_local(id, "Title A", "First note\nbody one", "Home", false, 2100);
    bool edited = false;
    e.transport.set_handler([&](const pbnotes::Request& r) {
        auto resp = e.server.handle(r);
        if (r.method == "PUT" && !edited) {   // the user renames the note after the request left
            edited = true;
            e.store->save_local(id, "Title B", "First note\nbody one", "Home", false, 2200);
        }
        return resp;
    });
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    auto n = e.store->get(id);
    PB_CHECK_EQ(n->title, std::string("Title B"));
    PB_CHECK(n->status == NoteStatus::Edited);          // only exactly what was sent is confirmed
    e.transport.set_handler([&](const pbnotes::Request& r) { return e.server.handle(r); });
    PB_CHECK(e.run().outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.notes.at(1).title, std::string("Title B"));
    PB_CHECK(e.store->get(id)->status == NoteStatus::Void);
}

PB_TEST(a_stored_title_that_breaks_the_rules_is_not_pushed_and_the_text_stays) {
    Env e;
    e.run();
    const auto id = e.local_of(1);
    e.store->save_local(id, "Fine", "First note\nchanged", "Home", false, 2100);
    PB_CHECK(e.store->debug_exec("UPDATE notes SET title='bad' || char(10) || 'title' WHERE local_id=" + std::to_string(id)));
    e.server.log.clear();
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Partial);
    PB_CHECK_EQ(e.server.requests_matching("PUT"), 0);
    PB_CHECK(e.store->get(id)->status == NoteStatus::Edited);
    PB_CHECK_EQ(e.store->get(id)->content, std::string("First note\nchanged"));
}

PB_TEST_MAIN()
