// T088: toggling a task checkbox is a normal local edit that the next sync pushes with If-Match.
#include <string>

#include "fake_notes_server.h"
#include "fake_transport.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "pbnotes/sync_engine.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::NoteStatus;
using pbnotes::SyncEngine;
using pbnotes::SyncOutcome;

PB_TEST(toggling_a_checkbox_is_saved_as_an_edit_and_synced) {
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
    server.add_on_server("Shopping\n- [ ] milk\n- [x] bread\n", "Home");
    SyncEngine engine(*store, transport, [] { return int64_t(1700001000); });
    PB_CHECK(engine.run().outcome == SyncOutcome::Success);

    const int64_t id = store->get_by_remote_id(1)->local_id;
    const std::string old_etag = *store->get(id)->etag;
    auto r = store->toggle_task(id, 1, 2000);  // tick "milk" (line index 1)
    PB_CHECK(r.error == pbnotes::EditError::None);
    auto n = store->get(id);
    PB_CHECK(n->status == NoteStatus::Edited);
    PB_CHECK_EQ(n->content, std::string("Shopping\n- [x] milk\n- [x] bread\n"));
    PB_CHECK_EQ(n->modified, static_cast<int64_t>(2000));

    auto sync = engine.run();
    PB_CHECK(sync.outcome == SyncOutcome::Success);
    PB_CHECK_EQ(sync.counts.pushed, 1);
    PB_CHECK_EQ(server.notes.at(1).content, std::string("Shopping\n- [x] milk\n- [x] bread\n"));
    bool put_with_if_match = false;
    for (const auto& req : transport.requests())
        if (req.method == "PUT") put_with_if_match = pbnotes::header_value(req.headers, "If-Match") == "\"" + old_etag + "\"";
    PB_CHECK(put_with_if_match);
    PB_CHECK(store->get(id)->status == NoteStatus::Void);
}

PB_TEST(toggling_something_that_is_not_a_task_changes_nothing) {
    pbtest::TmpDir dir;
    auto store = pbnotes::NoteStore::open(dir.file("db"));
    auto id = store->create_local(pbnotes::derive_title("Plain note\nno tasks here"), "Plain note\nno tasks here", "", false, 1000).local_id;
    auto r = store->toggle_task(id, 1, 2000);
    PB_CHECK(r.error == pbnotes::EditError::None);
    PB_CHECK_EQ(store->get(id)->modified, static_cast<int64_t>(1000));
    PB_CHECK(store->toggle_task(9999, 0, 1).error == pbnotes::EditError::NotFound);
}

PB_TEST_MAIN()
