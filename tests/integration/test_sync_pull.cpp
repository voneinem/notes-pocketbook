// T047: SyncEngine pull path against FakeTransport + a real NoteStore.
#include <fstream>
#include <iterator>
#include <string>

#include "fake_notes_server.h"
#include "fake_transport.h"
#include "pbnotes/note_store.h"
#include "pbnotes/sync_engine.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::ErrorClass;
using pbnotes::NoteStore;
using pbnotes::SyncEngine;
using pbnotes::SyncOutcome;
using pbtest::FakeTransport;

namespace {

std::string fixture(const std::string& name) {
    std::ifstream f(std::string(PBNOTES_FIXTURE_DIR) + "/" + name, std::ios::binary);
    if (!f) PB_FAIL_AT("missing fixture " + name);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

constexpr int64_t kNow = 1700001000;

struct Env {
    pbtest::TmpDir dir;
    std::unique_ptr<NoteStore> store;
    FakeTransport transport;
    Env() {
        store = NoteStore::open(dir.file("pbnotes.db"));
        pbnotes::Account a;
        a.server_url = "https://cloud.example.com";
        a.username = "alice";
        a.password = "app-password";
        store->save_account(a);
    }
    pbnotes::SyncResult run() {
        SyncEngine engine(*store, transport, [] { return kNow; });
        return engine.run();
    }
    void expect_capabilities() {
        transport.expect("GET", "capabilities", FakeTransport::ok(fixture("capabilities_ok.unverified.json")));
    }
    void expect_full_listing() {
        transport.expect("GET", "/notes",
                         FakeTransport::ok(fixture("notes_list_full.unverified.json"),
                                           {{"ETag", "\"coll-1\""},
                                            {"Last-Modified", "Tue, 14 Nov 2023 22:13:20 GMT"},
                                            {"X-Notes-API-Versions", "1.0, 1.3"}}));
    }
};

}  // namespace

PB_TEST(stored_titles_are_refreshed_by_one_full_pull_even_if_the_collection_is_unchanged) {
    Env e;
    e.store->apply_remote({[] { pbnotes::ApiNote n; n.id = 7; n.etag = "e7"; n.modified = 5; n.content = "# Local first line"; return n; }()});
    e.store->set_collection_state(std::string("\"coll-1\""), 1700000000, "1.0");
    pbtest::FakeNotesServer server;
    pbtest::FakeNotesServer::Note n;
    n.id = 7; n.etag = "e7"; n.modified = 5; n.content = "# Local first line"; n.title = "Server file name";
    server.notes[7] = n;
    e.transport.set_handler([&](const pbnotes::Request& r) { return server.handle(r); });
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.store->list()[0].title, std::string("Server file name"));
    // the first run asked for the full list: no pruneBefore and no If-None-Match
    bool full = false;
    for (const auto& q : e.transport.requests())
        if (q.url.find("/notes") != std::string::npos && q.url.find("capabilities") == std::string::npos)
            full = q.url.find("pruneBefore") == std::string::npos && !pbnotes::has_header(q.headers, "If-None-Match");
    PB_CHECK(full);
    // from now on the usual conditional listing is used again
    e.transport.clear_requests();
    e.run();
    bool conditional = false;
    for (const auto& q : e.transport.requests())
        if (q.url.find("/notes") != std::string::npos && q.url.find("capabilities") == std::string::npos)
            conditional = pbnotes::has_header(q.headers, "If-None-Match") || q.url.find("pruneBefore") != std::string::npos;
    PB_CHECK(conditional);
}

PB_TEST(first_pull_stores_all_notes_and_the_collection_bookkeeping) {
    Env e;
    e.expect_capabilities();
    e.expect_full_listing();
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.pulled, 3);
    PB_CHECK(e.transport.mismatches().empty());
    PB_CHECK_EQ(e.transport.requests().size(), static_cast<size_t>(2));
    const auto& list = e.transport.requests()[1];
    PB_CHECK(list.url.find("pruneBefore") == std::string::npos);  // nothing stored yet
    PB_CHECK(!pbnotes::has_header(list.headers, "If-None-Match"));
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(3));
    auto acc = e.store->get_account();
    PB_CHECK_EQ(acc->collection_etag.value_or(""), std::string("\"coll-1\""));
    PB_CHECK_EQ(acc->collection_last_modified, static_cast<int64_t>(1700000000));
    PB_CHECK_EQ(acc->api_versions, std::string("1.0, 1.3"));
    PB_CHECK_EQ(acc->last_sync_at.value_or(0), kNow);
    PB_CHECK(acc->last_sync_outcome == SyncOutcome::Success);
    PB_CHECK_EQ(acc->last_sync_counts.pulled, 3);
}

PB_TEST(incremental_pull_sends_validators_and_applies_the_changes) {
    Env e;
    e.expect_capabilities();
    e.expect_full_listing();
    e.run();

    e.transport.expect("GET", "pruneBefore=1700000000",
                       FakeTransport::ok(fixture("notes_list_pruned.unverified.json"), {{"ETag", "\"coll-2\""}}));
    auto r = e.run();
    const auto& req = e.transport.requests().back();
    PB_CHECK_EQ(pbnotes::header_value(req.headers, "If-None-Match"), std::string("\"coll-1\""));
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.pulled, 2);   // note 3 updated, note 4 inserted
    PB_CHECK_EQ(r.counts.deleted, 1);  // note 1 vanished from the server
    PB_CHECK(!e.store->get_by_remote_id(1).has_value());
    PB_CHECK_EQ(e.store->get_by_remote_id(3)->content, std::string("Ideas\nA note that was edited on the server.\n"));
    PB_CHECK(e.store->get_by_remote_id(4).has_value());
    PB_CHECK_EQ(e.store->get_account()->collection_etag.value_or(""), std::string("\"coll-2\""));
    // no capabilities request the second time: the stored API versions are used
    PB_CHECK_EQ(e.transport.requests().size(), static_cast<size_t>(3));
}

PB_TEST(not_modified_is_success_and_changes_nothing) {
    Env e;
    e.expect_capabilities();
    e.expect_full_listing();
    e.run();
    e.transport.expect("GET", "/notes", FakeTransport::status(304));
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.message_key, std::string("up_to_date"));
    PB_CHECK_EQ(r.counts.pulled, 0);
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(3));
}

PB_TEST(maintenance_mode_is_reported_without_touching_notes) {
    Env e;
    e.expect_capabilities();
    e.expect_full_listing();
    e.run();
    e.transport.expect("GET", "/notes", FakeTransport::status(503));
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Maintenance);
    PB_CHECK(r.error_class == ErrorClass::Maintenance);
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(3));
    PB_CHECK(e.store->get_account()->last_sync_outcome == SyncOutcome::Maintenance);
}

PB_TEST(no_network_is_offline_and_notes_stay_readable) {
    Env e;
    e.expect_capabilities();
    e.expect_full_listing();
    e.run();
    pbnotes::Response net;
    net.error = ErrorClass::Network;
    e.transport.expect("GET", "/notes", net);
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Offline);
    PB_CHECK_EQ(r.message_key, std::string("offline"));
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(3));
    auto n = e.store->get_by_remote_id(2);
    PB_CHECK(n.has_value());
    PB_CHECK(n->content.find("budget") != std::string::npos);  // still fully readable
}

PB_TEST(failures_are_classified_and_leave_the_store_alone) {
    struct Case {
        pbnotes::Response response;
        ErrorClass cls;
        const char* key;
    };
    pbnotes::Response tls;
    tls.error = ErrorClass::Tls;
    const Case cases[] = {
        {FakeTransport::status(401), ErrorClass::Auth, "auth_rejected"},
        {tls, ErrorClass::Tls, "certificate_unverifiable"},
        {FakeTransport::ok("not json"), ErrorClass::Server, "server_error"},
        {FakeTransport::status(500), ErrorClass::Server, "server_error"},
    };
    for (const auto& c : cases) {
        Env e;
        e.expect_capabilities();
        e.expect_full_listing();
        e.run();
        e.transport.expect("GET", "/notes", c.response);
        auto r = e.run();
        PB_CHECK(r.outcome == SyncOutcome::Failed);
        PB_CHECK(r.error_class == c.cls);
        PB_CHECK_EQ(r.message_key, std::string(c.key));
        PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(3));
    }
}

PB_TEST(server_without_api_v1_makes_no_note_request) {
    Env e;
    e.transport.expect("GET", "capabilities", FakeTransport::ok(fixture("capabilities_only_v02.unverified.json")));
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Failed);
    PB_CHECK(r.error_class == ErrorClass::Unsupported);
    PB_CHECK_EQ(r.message_key, std::string("server_unsupported"));
    PB_CHECK_EQ(e.transport.requests().size(), static_cast<size_t>(1));  // capabilities only
}

PB_TEST(a_pruned_entry_for_an_unknown_note_is_fetched_individually) {
    Env e;
    e.expect_capabilities();
    e.transport.expect("GET", "/notes", FakeTransport::ok("[{\"id\":9}]", {{"X-Notes-API-Versions", "1.0"}}));
    std::string body = fixture("note_single.unverified.json");
    body.replace(body.find("\"id\":4"), 6, "\"id\":9");  // the server answers with note 9
    e.transport.expect("GET", "/notes/9", FakeTransport::ok(body));
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.pulled, 1);
    PB_CHECK(e.store->get_by_remote_id(9).has_value());
}

PB_TEST(a_fetched_note_with_the_wrong_id_is_a_server_error) {
    Env e;
    e.expect_capabilities();
    e.transport.expect("GET", "/notes", FakeTransport::ok("[{\"id\":9}]", {{"X-Notes-API-Versions", "1.0"}}));
    e.transport.expect("GET", "/notes/9", FakeTransport::ok(fixture("note_single.unverified.json")));  // says id 4
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Failed);
    PB_CHECK(r.error_class == ErrorClass::Server);
    PB_CHECK(!e.store->get_by_remote_id(9).has_value());
    PB_CHECK(!e.store->get_by_remote_id(4).has_value());
}

PB_TEST(without_an_account_nothing_is_sent) {
    pbtest::TmpDir dir;
    auto store = NoteStore::open(dir.file("pbnotes.db"));
    FakeTransport transport;
    SyncEngine engine(*store, transport, [] { return kNow; });
    auto r = engine.run();
    PB_CHECK(r.outcome == SyncOutcome::Failed);
    PB_CHECK_EQ(r.message_key, std::string("no_account"));
    PB_CHECK(transport.requests().empty());
}

PB_TEST(a_second_run_during_a_run_is_refused) {
    Env e;
    SyncEngine engine(*e.store, e.transport, [] { return kNow; });
    pbnotes::SyncResult nested;
    bool nested_done = false;
    e.transport.set_handler([&](const pbnotes::Request& req) -> pbnotes::Response {
        if (!nested_done) {
            nested_done = true;
            nested = engine.run();  // re-entrant request while the first run is in flight
        }
        if (req.url.find("capabilities") != std::string::npos) return FakeTransport::ok(fixture("capabilities_ok.unverified.json"));
        return FakeTransport::ok("[]", {{"X-Notes-API-Versions", "1.0"}});
    });
    auto r = engine.run();
    PB_CHECK(nested_done);
    PB_CHECK_EQ(nested.message_key, std::string("already_running"));
    PB_CHECK(nested.outcome == SyncOutcome::Running);
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK(!engine.is_running());
}

PB_TEST_MAIN()
