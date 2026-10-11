// T097: host portion of SC-009 — syncing a thousand notes where only a few changed is quick
// (network time on the device is measured separately, quickstart step 6).
#include <chrono>
#include <string>

#include "fake_notes_server.h"
#include "fake_transport.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "pbnotes/sync_engine.h"
#include "test_support.h"
#include "tmp_dir.h"

PB_TEST(syncing_a_thousand_notes_with_few_changes_is_quick) {
    pbtest::TmpDir dir;
    auto store = pbnotes::NoteStore::open(dir.file("db"));
    pbnotes::Account a;
    a.server_url = "https://cloud.example.com";
    a.username = "alice";
    a.password = "pw";
    store->save_account(a);
    pbtest::FakeNotesServer server;
    pbtest::FakeTransport transport;
    transport.set_handler([&](const pbnotes::Request& r) { return server.handle(r); });
    std::string body;
    for (int i = 0; i < 40; ++i) body += "line " + std::to_string(i) + " of ordinary note text, a few words long\n";
    for (int i = 1; i <= 1000; ++i) server.add_on_server("Note " + std::to_string(i) + "\n" + body, i % 5 == 0 ? "Work" : "");
    pbnotes::SyncEngine engine(*store, transport, [] { return int64_t(1700001000); });

    const auto t0 = std::chrono::steady_clock::now();
    auto first = engine.run();
    const auto first_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    PB_CHECK(first.outcome == pbnotes::SyncOutcome::Success);
    PB_CHECK_EQ(first.counts.pulled, 1000);

    // a few changes on each side
    for (int i : {10, 20, 30}) store->save_local(store->get_by_remote_id(i)->local_id, pbnotes::derive_title("Note " + std::to_string(i) + "\nedited here"), "Note " + std::to_string(i) + "\nedited here", "", false, 2000 + i);
    for (int i : {40, 50}) server.edit_on_server(i, "Note " + std::to_string(i) + "\nedited there");

    const auto t1 = std::chrono::steady_clock::now();
    auto second = engine.run();
    const auto second_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t1).count();
    PB_CHECK(second.outcome == pbnotes::SyncOutcome::Success);
    PB_CHECK_EQ(second.counts.pushed, 3);
    PB_CHECK(second.counts.pulled >= 2);
    PB_CHECK(first_ms < 5000);
    PB_CHECK(second_ms < 2000);
}

PB_TEST_MAIN()
