// T079: deleted-versus-edited in both directions, plus the DELETE variants.
#include <fstream>
#include <iterator>
#include <string>

#include "fake_notes_server.h"
#include "fake_transport.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "pbnotes/sync_engine.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::NoteStatus;
using pbnotes::NoteStore;
using pbnotes::SyncEngine;
using pbnotes::SyncOutcome;

namespace {

constexpr int64_t kDay = 1791633600;

struct Env {
    pbtest::TmpDir dir;
    std::unique_ptr<NoteStore> store;
    pbtest::FakeNotesServer server;
    pbtest::FakeTransport transport;

    Env() {
        store = NoteStore::open(dir.file("pbnotes.db"));
        pbnotes::Account a;
        a.server_url = "https://cloud.example.com";
        a.username = "alice";
        a.password = "app-password";
        store->save_account(a);
        transport.set_handler([this](const pbnotes::Request& r) { return server.handle(r); });
        server.add_on_server("First note\nbody one");
        server.add_on_server("Second note\nbody two");
        run();
    }
    pbnotes::SyncResult run(SyncEngine::Options o = {}) {
        SyncEngine engine(*store, transport, [] { return kDay; }, o);
        return engine.run();
    }
    int64_t local_of(int64_t remote_id) { return store->get_by_remote_id(remote_id)->local_id; }
};

}  // namespace

PB_TEST(edited_locally_but_deleted_on_the_server_is_recreated) {
    Env e;
    const int64_t id = e.local_of(1);
    e.store->save_local(id, pbnotes::derive_title("First note\nedited after the remote delete"), "First note\nedited after the remote delete", "", false, 2000);
    e.server.notes.erase(1);
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.recreated, 1);
    PB_CHECK_EQ(e.server.count_with_content("First note\nedited after the remote delete"), 1);
    PB_CHECK(e.store->get(id)->status == NoteStatus::Void);
}

PB_TEST(deleted_locally_but_changed_on_the_server_is_restored_not_deleted) {
    Env e;
    const int64_t id = e.local_of(2);
    e.store->mark_deleted(id);
    e.server.edit_on_server(2, "Second note\nsomeone improved this");
    e.server.log.clear();
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.restored, 1);  // the user is told
    PB_CHECK_EQ(e.server.requests_matching("DELETE"), 0);
    PB_CHECK_EQ(e.server.notes.at(2).content, std::string("Second note\nsomeone improved this"));
    auto n = e.store->get(id);
    PB_CHECK(n.has_value());
    PB_CHECK(n->status == NoteStatus::Void);  // back in the list with the server's version
    PB_CHECK_EQ(n->content, std::string("Second note\nsomeone improved this"));
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(2));
}

PB_TEST(deleted_locally_and_unchanged_on_the_server_is_deleted) {
    Env e;
    const int64_t id = e.local_of(2);
    e.store->mark_deleted(id);
    e.server.log.clear();
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.requests_matching("DELETE /notes/2"), 1);
    PB_CHECK(!e.store->get(id).has_value());
    PB_CHECK(e.server.notes.count(2) == 0);
}

PB_TEST(deleted_locally_and_already_gone_on_the_server_is_just_removed) {
    Env e;
    const int64_t id = e.local_of(2);
    e.store->mark_deleted(id);
    e.server.notes.erase(2);
    e.server.log.clear();
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.requests_matching("DELETE"), 0);
    PB_CHECK(!e.store->get(id).has_value());
}

PB_TEST(the_if_match_variant_deletes_with_a_single_conditional_request) {
    Env e;
    e.server.honor_if_match_on_delete = true;
    const int64_t id = e.local_of(2);
    e.store->mark_deleted(id);
    e.server.log.clear();
    SyncEngine::Options o;
    o.delete_uses_if_match = true;
    auto r = e.run(o);
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.requests_matching("GET /notes/2"), 0);  // no separate check
    PB_CHECK_EQ(e.server.requests_matching("DELETE /notes/2"), 1);
    bool had_if_match = false;
    for (const auto& req : e.transport.requests())
        if (req.method == "DELETE") had_if_match = pbnotes::has_header(req.headers, "If-Match");
    PB_CHECK(had_if_match);
    PB_CHECK(!e.store->get(id).has_value());
}

PB_TEST(the_if_match_variant_restores_a_note_that_changed_meanwhile) {
    Env e;
    e.server.honor_if_match_on_delete = true;
    const int64_t id = e.local_of(2);
    e.store->mark_deleted(id);
    e.server.edit_on_server(2, "Second note\nchanged elsewhere");
    SyncEngine::Options o;
    o.delete_uses_if_match = true;
    auto r = e.run(o);  // DELETE answers 412, the engine fetches and restores the server version
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.restored, 1);
    PB_CHECK(e.server.notes.count(2) == 1);
    PB_CHECK_EQ(e.store->get(id)->content, std::string("Second note\nchanged elsewhere"));
}

PB_TEST(a_restored_note_can_be_edited_and_synced_again) {
    Env e;
    const int64_t id = e.local_of(2);
    e.store->mark_deleted(id);
    e.server.edit_on_server(2, "Second note\nsomeone improved this");
    e.run();
    PB_CHECK(e.store->save_local(id, pbnotes::derive_title("Second note\nmy follow-up"), "Second note\nmy follow-up", "", false, 3000).error == pbnotes::EditError::None);
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.notes.at(2).content, std::string("Second note\nmy follow-up"));
}

PB_TEST_MAIN()
