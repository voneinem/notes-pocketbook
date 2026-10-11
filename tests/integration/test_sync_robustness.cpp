// T069: interrupted syncs never lose or corrupt a note; a note edited while its push is in flight
// stays pending; error classes leave local data alone. (SC-005, FR-029, FR-036.)
#include <set>
#include <string>
#include <vector>

#include "fake_notes_server.h"
#include "fake_transport.h"
#include "pbnotes/note_store.h"
#include "pbnotes/sync_engine.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::ErrorClass;
using pbnotes::NoteStatus;
using pbnotes::NoteStore;
using pbnotes::SyncEngine;
using pbnotes::SyncOutcome;

namespace {

const char* kNewText = "Brand new\nfrom the device";
const char* kEditText = "First note\nedited on the device";

struct Env {
    pbtest::TmpDir dir;
    std::unique_ptr<NoteStore> store;
    pbtest::FakeNotesServer server;
    pbtest::FakeTransport transport;
    int64_t new_id = 0, edit_id = 0, delete_id = 0;

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
        SyncEngine engine(*store, transport, [] { return int64_t(1700001000); });
        return engine.run();
    }
    // Baseline sync plus one new, one edited and one deleted note.
    void prepare() {
        run();
        new_id = store->create_local(pbnotes::derive_title(kNewText), kNewText, "Work", false, 2000).local_id;
        edit_id = store->get_by_remote_id(1)->local_id;
        store->save_local(edit_id, pbnotes::derive_title(kEditText), kEditText, "Home", false, 2100);
        delete_id = store->get_by_remote_id(2)->local_id;
        store->mark_deleted(delete_id);
    }
    bool has_local_content(const std::string& content) const {
        for (const auto& s : store->list()) {
            auto n = store->get(s.local_id);
            if (n && n->content == content) return true;
        }
        return false;
    }
};

// Number of requests a clean sync of the prepared state needs.
size_t clean_run_length() {
    Env e;
    e.prepare();
    const size_t before = e.transport.requests().size();
    e.run();
    return e.transport.requests().size() - before;
}

}  // namespace

PB_TEST(a_dropped_request_at_any_point_loses_nothing_and_the_next_run_converges) {
    const size_t total = clean_run_length();
    PB_CHECK(total >= 5);
    for (size_t k = 0; k < total; ++k) {
        Env e;
        e.prepare();
        e.transport.fail_request(e.transport.requests().size() + k, ErrorClass::Network);  // the request never arrives
        auto broken = e.run();
        PB_CHECK(broken.outcome == SyncOutcome::Offline);

        // nothing lost, nothing corrupted
        PB_CHECK(e.has_local_content(kNewText));
        PB_CHECK(e.has_local_content(kEditText));
        for (const auto& s : e.store->list()) PB_CHECK_EQ(s.title, pbnotes::derive_title(e.store->get(s.local_id)->content));

        auto healed = e.run();
        if (healed.outcome != SyncOutcome::Success) PB_FAIL_AT("did not converge after a drop at request " + std::to_string(k));
        PB_CHECK_EQ(e.server.count_with_content(kNewText), 1);  // no duplicate when the request never arrived
        PB_CHECK_EQ(e.server.notes.size(), e.store->list().size());
        for (const auto& s : e.store->list()) PB_CHECK(!s.pending);
        PB_CHECK_EQ(e.server.count_with_content(kEditText), 1);
        PB_CHECK(e.server.notes.count(2) == 0);  // the delete went through
    }
}

PB_TEST(a_lost_response_at_any_point_loses_nothing) {
    const size_t total = clean_run_length();
    for (size_t k = 0; k < total; ++k) {
        Env e;
        e.prepare();
        const size_t fail_at = e.transport.requests().size() + k;
        bool was_post = false;
        e.transport.set_handler([&](const pbnotes::Request& req) {
            pbnotes::Response resp = e.server.handle(req);  // the server DID process it
            if (e.transport.requests().size() - 1 == fail_at) {
                was_post = req.method == "POST";
                pbnotes::Response lost;
                lost.error = ErrorClass::Network;  // ...but the client never saw the answer
                return lost;
            }
            return resp;
        });
        e.run();

        PB_CHECK(e.has_local_content(kNewText));
        PB_CHECK(e.has_local_content(kEditText));
        for (const auto& s : e.store->list()) PB_CHECK_EQ(s.title, pbnotes::derive_title(e.store->get(s.local_id)->content));

        e.transport.set_handler([&](const pbnotes::Request& req) { return e.server.handle(req); });
        e.run();
        // A duplicate is accepted only when the lost response belonged to a POST; both copies are identical.
        const int copies = e.server.count_with_content(kNewText);
        PB_CHECK(copies >= 1);
        if (copies > 1) PB_CHECK(was_post);
        PB_CHECK(e.server.count_with_content(kEditText) >= 1);  // the edit reached the server
        PB_CHECK(e.has_local_content(kNewText));
        PB_CHECK(e.has_local_content(kEditText));
    }
}

PB_TEST(a_note_edited_while_its_put_is_in_flight_stays_pending) {
    Env e;
    e.run();
    auto id = e.store->get_by_remote_id(1)->local_id;
    e.store->save_local(id, pbnotes::derive_title("First note\nsent text"), "First note\nsent text", "Home", false, 2100);
    bool edited_midflight = false;
    e.transport.set_handler([&](const pbnotes::Request& req) {
        auto resp = e.server.handle(req);
        if (req.method == "PUT" && !edited_midflight) {
            edited_midflight = true;
            e.store->save_local(id, pbnotes::derive_title("First note\ntyped while syncing"), "First note\ntyped while syncing", "Home", false, 2200);  // the user keeps typing
        }
        return resp;
    });
    e.run();
    auto n = e.store->get(id);
    PB_CHECK(edited_midflight);
    PB_CHECK(n->status == NoteStatus::Edited);                            // FR-036: not confirmed yet
    PB_CHECK_EQ(n->content, std::string("First note\ntyped while syncing"));  // the newer text survived
    PB_CHECK_EQ(n->etag.value_or(""), e.server.notes.at(1).etag);         // but the validator is current

    e.transport.set_handler([&](const pbnotes::Request& req) { return e.server.handle(req); });
    auto again = e.run();
    PB_CHECK(again.outcome == SyncOutcome::Success);
    PB_CHECK(e.store->get(id)->status == NoteStatus::Void);
    PB_CHECK_EQ(e.server.notes.at(1).content, std::string("First note\ntyped while syncing"));
}

PB_TEST(a_note_edited_while_its_post_is_in_flight_keeps_the_newer_text_and_gets_its_identity) {
    Env e;
    e.run();
    auto id = e.store->create_local(pbnotes::derive_title("Fresh\nsent text"), "Fresh\nsent text", "", false, 2000).local_id;
    bool edited_midflight = false;
    e.transport.set_handler([&](const pbnotes::Request& req) {
        auto resp = e.server.handle(req);
        if (req.method == "POST" && !edited_midflight) {
            edited_midflight = true;
            e.store->save_local(id, pbnotes::derive_title("Fresh\ntyped while syncing"), "Fresh\ntyped while syncing", "", false, 2200);
        }
        return resp;
    });
    e.run();
    auto n = e.store->get(id);
    PB_CHECK(n->status == NoteStatus::Edited);
    PB_CHECK(n->remote_id.has_value());  // so the next push is a PUT, not a second POST
    e.transport.set_handler([&](const pbnotes::Request& req) { return e.server.handle(req); });
    e.run();
    PB_CHECK(e.store->get(id)->status == NoteStatus::Void);
    PB_CHECK_EQ(e.server.count_with_content("Fresh\ntyped while syncing"), 1);
    PB_CHECK_EQ(e.server.count_with_content("Fresh\nsent text"), 0);
}

PB_TEST(error_classes_leave_the_pending_changes_untouched) {
    struct Case {
        pbnotes::Response response;
        SyncOutcome outcome;
        ErrorClass cls;
    };
    pbnotes::Response tls;
    tls.error = ErrorClass::Tls;
    pbnotes::Response net;
    net.error = ErrorClass::Network;
    const Case cases[] = {
        {pbtest::FakeTransport::status(401), SyncOutcome::Failed, ErrorClass::Auth},
        {tls, SyncOutcome::Failed, ErrorClass::Tls},
        {net, SyncOutcome::Offline, ErrorClass::Network},
        {pbtest::FakeTransport::status(503), SyncOutcome::Maintenance, ErrorClass::Maintenance},
    };
    for (const auto& c : cases) {
        Env e;
        e.prepare();
        e.transport.set_handler([&](const pbnotes::Request&) { return c.response; });
        auto r = e.run();
        PB_CHECK(r.outcome == c.outcome);
        PB_CHECK(r.error_class == c.cls);
        PB_CHECK(e.has_local_content(kNewText));
        PB_CHECK(e.has_local_content(kEditText));
        PB_CHECK(e.store->get(e.delete_id).has_value());
        PB_CHECK_EQ(e.store->get(e.new_id)->status == NoteStatus::Edited ? 1 : 0, 1);
    }
}

PB_TEST(not_modified_listing_after_a_clean_push_is_a_success) {
    Env e;
    e.prepare();
    e.run();
    // nothing changed on the server since our own push: the listing may answer 304
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
}

PB_TEST_MAIN()
