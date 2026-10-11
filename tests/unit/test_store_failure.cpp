// T060: when a write fails (the storage is full) the previous version of the note is preserved
// and the error is reported (edge case: the device runs out of storage).
#include <string>

#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::EditError;
using pbnotes::NoteStatus;
using pbnotes::NoteStore;

PB_TEST(a_failed_save_keeps_the_previous_version_and_reports_the_error) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto id = store->create_local(pbnotes::derive_title("Precious\nOriginal body"), "Precious\nOriginal body", "Work", true, 1000).local_id;
    // Simulate a full disk: no more pages may be added to the database.
    const std::string pages = store->pragma("page_count");
    PB_CHECK(store->debug_exec("PRAGMA max_page_count=" + pages));

    const std::string huge(2 * 1024 * 1024, 'x');
    auto r = store->save_local(id, pbnotes::derive_title("Precious\n" + huge), "Precious\n" + huge, "Work", true, 2000);
    PB_CHECK(r.error == EditError::StorageError);

    auto n = store->get(id);
    PB_CHECK(n.has_value());
    PB_CHECK_EQ(n->content, std::string("Precious\nOriginal body"));
    PB_CHECK_EQ(n->modified, static_cast<int64_t>(1000));
    PB_CHECK(n->status == NoteStatus::Edited);
}

PB_TEST(a_failed_create_stores_nothing) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    PB_CHECK(store->create_local(pbnotes::derive_title("First"), "First", "", false, 1000).error == EditError::None);
    PB_CHECK(store->debug_exec("PRAGMA max_page_count=" + store->pragma("page_count")));
    auto r = store->create_local(pbnotes::derive_title("Second\n" + std::string(2 * 1024 * 1024, 'y')), "Second\n" + std::string(2 * 1024 * 1024, 'y'), "", false, 1100);
    PB_CHECK(r.error == EditError::StorageError);
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(1));
}

PB_TEST(a_failed_delete_keeps_the_note) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({[] {
        pbnotes::ApiNote n;
        n.id = 1;
        n.etag = "e";
        n.content = "Synced\nbody";
        n.modified = 5;
        return n;
    }()});
    auto id = store->get_by_remote_id(1)->local_id;
    PB_CHECK(store->debug_exec("PRAGMA query_only=ON"));  // every write now fails
    auto r = store->mark_deleted(id);
    PB_CHECK(r.error == EditError::StorageError);
    PB_CHECK(store->debug_exec("PRAGMA query_only=OFF"));
    PB_CHECK(store->get(id)->status == NoteStatus::Void);
}

PB_TEST_MAIN()
