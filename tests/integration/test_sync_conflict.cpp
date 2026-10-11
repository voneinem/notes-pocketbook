// T077: both sides changed a note — the server's version stays the original, the device's version
// becomes a clearly named conflict copy that is uploaded; nothing is overwritten or lost.
#include <string>

#include "fake_notes_server.h"
#include "fake_transport.h"
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

constexpr int64_t kDay = 1791633600;  // 2026-10-10 12:00:00 UTC

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
        server.add_on_server("Meeting notes\nagenda", "Work");
        server.add_on_server("Second note\nbody two");
        run();  // baseline
    }
    pbnotes::SyncResult run() {
        SyncEngine engine(*store, transport, [] { return kDay; });
        return engine.run();
    }
    int64_t local_of(int64_t remote_id) { return store->get_by_remote_id(remote_id)->local_id; }
    // Every local note whose title starts with `prefix` (conflict copies carry the marker in the title).
    std::vector<pbnotes::Note> titled_starting_with(const std::string& prefix) {
        std::vector<pbnotes::Note> out;
        for (const auto& s : store->list()) {
            auto n = store->get(s.local_id);
            if (n->title.compare(0, prefix.size(), prefix) == 0) out.push_back(*n);
        }
        return out;
    }
    // Every local note whose content starts with `prefix`.
    std::vector<pbnotes::Note> notes_starting_with(const std::string& prefix) {
        std::vector<pbnotes::Note> out;
        for (const auto& s : store->list()) {
            auto n = store->get(s.local_id);
            if (n->content.compare(0, prefix.size(), prefix) == 0) out.push_back(*n);
        }
        return out;
    }
};

}  // namespace

PB_TEST(both_versions_are_kept_and_the_copy_is_uploaded) {
    Env e;
    e.store->save_local(e.local_of(1), pbnotes::derive_title("Meeting notes\nmy local edit"), "Meeting notes\nmy local edit", "Work", false, 2000);
    e.server.edit_on_server(1, "Meeting notes\nsomeone else's edit");
    e.server.log.clear();

    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.conflicts, 1);

    // the server's version stays the original, on the device and on the server
    PB_CHECK_EQ(e.server.notes.at(1).content, std::string("Meeting notes\nsomeone else's edit"));
    auto original = e.store->get(e.local_of(1));
    PB_CHECK_EQ(original->content, std::string("Meeting notes\nsomeone else's edit"));
    PB_CHECK(original->status == NoteStatus::Void);

    // the device's version survives as a clearly named copy that also exists on the server
    auto copies = e.titled_starting_with("Meeting notes (conflict 2026-10-10)");
    PB_CHECK_EQ(copies.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(copies[0].title, std::string("Meeting notes (conflict 2026-10-10)"));
    PB_CHECK_EQ(copies[0].content, std::string("Meeting notes\nmy local edit"));   // the text is untouched (spec 004)
    PB_CHECK(copies[0].status == NoteStatus::Void);
    PB_CHECK(copies[0].remote_id.has_value());
    PB_CHECK_EQ(e.server.count_with_content("Meeting notes\nmy local edit"), 1);
    PB_CHECK_EQ(e.server.notes.at(*copies[0].remote_id).title, std::string("Meeting notes (conflict 2026-10-10)"));

    // exactly: PUT (rejected with 412), GET of the server version, POST of the copy
    PB_CHECK_EQ(e.server.requests_matching("PUT /notes/1"), 1);
    PB_CHECK_EQ(e.server.requests_matching("GET /notes/1"), 1);
    PB_CHECK_EQ(e.server.requests_matching("POST /notes"), 1);
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(3));
}

PB_TEST(running_again_creates_no_further_copy) {
    Env e;
    e.store->save_local(e.local_of(1), pbnotes::derive_title("Meeting notes\nmy local edit"), "Meeting notes\nmy local edit", "Work", false, 2000);
    e.server.edit_on_server(1, "Meeting notes\nsomeone else's edit");
    e.run();
    auto again = e.run();
    PB_CHECK(again.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(again.counts.conflicts, 0);
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(3));
    PB_CHECK_EQ(e.server.notes.size(), static_cast<size_t>(3));
}

PB_TEST(a_dropped_connection_while_fetching_the_server_version_changes_nothing_and_is_retried) {
    Env e;
    e.store->save_local(e.local_of(1), pbnotes::derive_title("Meeting notes\nmy local edit"), "Meeting notes\nmy local edit", "Work", false, 2000);
    e.server.edit_on_server(1, "Meeting notes\nsomeone else's edit");
    // requests of the next run: PUT (412), then the GET of the server version -> drop that one
    e.transport.fail_request(e.transport.requests().size() + 1, ErrorClass::Network);
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Offline);
    auto n = e.store->get(e.local_of(1));
    PB_CHECK(n->status == NoteStatus::Edited);
    PB_CHECK_EQ(n->content, std::string("Meeting notes\nmy local edit"));
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(2));  // no copy yet

    auto retry = e.run();
    PB_CHECK(retry.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(3));  // exactly one copy
}

PB_TEST(a_failed_upload_of_the_copy_leaves_it_pending_and_the_next_run_uploads_it) {
    Env e;
    e.store->save_local(e.local_of(1), pbnotes::derive_title("Meeting notes\nmy local edit"), "Meeting notes\nmy local edit", "Work", false, 2000);
    e.server.edit_on_server(1, "Meeting notes\nsomeone else's edit");
    bool failed_once = false;
    e.transport.set_handler([&](const pbnotes::Request& req) {
        if (req.method == "POST" && !failed_once) {
            failed_once = true;
            return pbtest::FakeTransport::status(500);
        }
        return e.server.handle(req);
    });
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Partial);
    auto copies = e.titled_starting_with("Meeting notes (conflict");
    PB_CHECK_EQ(copies.size(), static_cast<size_t>(1));
    PB_CHECK(copies[0].status == NoteStatus::Edited);  // nothing lost, still to be uploaded
    PB_CHECK(!copies[0].remote_id.has_value());

    e.transport.set_handler([&](const pbnotes::Request& req) { return e.server.handle(req); });
    auto next = e.run();
    PB_CHECK(next.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(e.server.count_with_content("Meeting notes\nmy local edit"), 1);
    PB_CHECK_EQ(e.titled_starting_with("Meeting notes (conflict")[0].status == NoteStatus::Void ? 1 : 0, 1);
}

PB_TEST(identical_edits_on_both_sides_create_no_copy) {
    Env e;
    e.store->save_local(e.local_of(1), pbnotes::derive_title("Meeting notes\nthe same edit"), "Meeting notes\nthe same edit", "Work", false, 2000);
    e.server.edit_on_server(1, "Meeting notes\nthe same edit");
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.conflicts, 0);
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(2));
    PB_CHECK(e.store->get(e.local_of(1))->status == NoteStatus::Void);
}

PB_TEST(a_second_conflict_on_the_same_day_gets_a_numbered_copy) {
    Env e;
    for (int round = 1; round <= 2; ++round) {
        e.store->save_local(e.local_of(1), pbnotes::derive_title("Meeting notes\nlocal round " + std::to_string(round)), "Meeting notes\nlocal round " + std::to_string(round), "Work", false, 2000 + round);
        e.server.edit_on_server(1, "Meeting notes\nserver round " + std::to_string(round));
        auto r = e.run();
        PB_CHECK(r.outcome == SyncOutcome::Success);
        PB_CHECK_EQ(r.counts.conflicts, 1);
    }
    PB_CHECK_EQ(e.titled_starting_with("Meeting notes (conflict 2026-10-10)").size(), static_cast<size_t>(1));
    PB_CHECK_EQ(e.titled_starting_with("Meeting notes (conflict 2026-10-10 #2)").size(), static_cast<size_t>(1));
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(4));
}

PB_TEST(conflicts_are_kept_apart_per_note) {
    Env e;
    e.store->save_local(e.local_of(1), pbnotes::derive_title("Meeting notes\nlocal A"), "Meeting notes\nlocal A", "Work", false, 2000);
    e.store->save_local(e.local_of(2), pbnotes::derive_title("Second note\nlocal B"), "Second note\nlocal B", "", false, 2001);
    e.server.edit_on_server(1, "Meeting notes\nserver A");
    e.server.edit_on_server(2, "Second note\nserver B");
    auto r = e.run();
    PB_CHECK_EQ(r.counts.conflicts, 2);
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(4));
    PB_CHECK_EQ(e.server.count_with_content("Meeting notes\nlocal A"), 1);
    PB_CHECK_EQ(e.server.count_with_content("Second note\nlocal B"), 1);
    PB_CHECK_EQ(e.titled_starting_with("Meeting notes (conflict 2026-10-10)").size(), static_cast<size_t>(1));
    PB_CHECK_EQ(e.titled_starting_with("Second note (conflict 2026-10-10)").size(), static_cast<size_t>(1));
}

// ---- spec 004: the title is part of what can conflict ---------------------------------------------------------------

PB_TEST(a_title_only_edit_on_the_device_and_a_text_edit_on_the_server_keep_both) {
    Env e;
    e.store->save_local(e.local_of(1), "My own title", "Meeting notes\nagenda", "Work", false, 2000);   // only the title changed
    e.server.edit_on_server(1, "Meeting notes\nsomeone else's edit");
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.conflicts, 1);
    PB_CHECK_EQ(e.server.notes.at(1).content, std::string("Meeting notes\nsomeone else's edit"));   // not overwritten
    auto copies = e.titled_starting_with("My own title (conflict 2026-10-10)");
    PB_CHECK_EQ(copies.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(copies[0].content, std::string("Meeting notes\nagenda"));
    PB_CHECK(copies[0].status == NoteStatus::Void);                        // uploaded
    PB_CHECK_EQ(e.server.notes.at(*copies[0].remote_id).title, std::string("My own title (conflict 2026-10-10)"));
}

PB_TEST(a_title_only_edit_on_both_sides_keeps_both_titles) {
    Env e;
    e.store->save_local(e.local_of(1), "Device title", "Meeting notes\nagenda", "Work", false, 2000);
    e.server.rename_on_server(1, "Server title");
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.conflicts, 1);
    PB_CHECK_EQ(e.store->get(e.local_of(1))->title, std::string("Server title"));
    PB_CHECK_EQ(e.titled_starting_with("Device title (conflict 2026-10-10)").size(), static_cast<size_t>(1));
}

PB_TEST(a_title_changed_on_the_server_reaches_a_note_without_local_changes) {
    Env e;
    e.server.rename_on_server(1, "Renamed elsewhere");
    auto r = e.run();
    PB_CHECK(r.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(r.counts.conflicts, 0);
    PB_CHECK_EQ(e.store->get(e.local_of(1))->title, std::string("Renamed elsewhere"));
    PB_CHECK_EQ(e.store->get(e.local_of(1))->content, std::string("Meeting notes\nagenda"));
    PB_CHECK_EQ(e.store->list().size(), static_cast<size_t>(2));
}

PB_TEST_MAIN()
