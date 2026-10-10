// T058: local create / save / delete — states, titles, validation, persistence.
#include <string>

#include "pbnotes/note_store.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::ApiNote;
using pbnotes::EditError;
using pbnotes::NoteStatus;
using pbnotes::NoteStore;

namespace {
ApiNote server_note(int64_t id, const std::string& content) {
    ApiNote n;
    n.id = id;
    n.etag = "e" + std::to_string(id);
    n.content = content;
    n.modified = 100;
    return n;
}
}  // namespace

PB_TEST(new_notes_must_have_content) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    for (const char* empty : {"", "   ", "\n\n", " \t \r\n "}) {
        auto r = store->create_local(empty, "", false, 1000);
        PB_CHECK(r.error == EditError::EmptyContent);
    }
    PB_CHECK(store->list().empty());  // nothing was stored (FR-020)
}

PB_TEST(create_local_makes_a_pending_note_without_remote_identity) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto r = store->create_local("Groceries\n- milk\n- eggs", "Home", true, 1000);
    PB_CHECK(r.error == EditError::None);
    auto n = store->get(r.local_id);
    PB_CHECK(n.has_value());
    PB_CHECK(n->status == NoteStatus::Edited);
    PB_CHECK(!n->remote_id.has_value());
    PB_CHECK(!n->etag.has_value());
    PB_CHECK_EQ(n->title, std::string("Groceries"));
    PB_CHECK_EQ(n->excerpt, std::string("- milk - eggs"));
    PB_CHECK_EQ(n->category, std::string("Home"));
    PB_CHECK(n->favorite);
    PB_CHECK_EQ(n->modified, static_cast<int64_t>(1000));
    auto list = store->list();
    PB_CHECK_EQ(list.size(), static_cast<size_t>(1));
    PB_CHECK(list[0].pending);
}

PB_TEST(saving_a_synced_note_marks_it_edited_and_recomputes_the_title) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Old title\nbody")});
    auto id = store->get_by_remote_id(1)->local_id;
    auto r = store->save_local(id, "New title\nnew body", "Work", false, 2000);
    PB_CHECK(r.error == EditError::None);
    auto n = store->get(id);
    PB_CHECK(n->status == NoteStatus::Edited);
    PB_CHECK_EQ(n->title, std::string("New title"));
    PB_CHECK_EQ(n->excerpt, std::string("new body"));
    PB_CHECK_EQ(n->modified, static_cast<int64_t>(2000));
    PB_CHECK(n->remote_id.has_value());          // identity and validator are kept for the push
    PB_CHECK_EQ(n->etag.value_or(""), std::string("e1"));
}

PB_TEST(saving_identical_values_is_not_a_change) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Same\nbody")});
    auto id = store->get_by_remote_id(1)->local_id;
    PB_CHECK(store->save_local(id, "Same\nbody", "", false, 5000).error == EditError::None);
    auto n = store->get(id);
    PB_CHECK(n->status == NoteStatus::Void);  // nothing to push
    PB_CHECK_EQ(n->modified, static_cast<int64_t>(100));
}

PB_TEST(saving_empty_content_is_rejected_and_keeps_the_note) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Keep me\nbody")});
    auto id = store->get_by_remote_id(1)->local_id;
    PB_CHECK(store->save_local(id, "  \n ", "", false, 2000).error == EditError::EmptyContent);
    PB_CHECK_EQ(store->get(id)->content, std::string("Keep me\nbody"));
}

PB_TEST(favorite_and_category_changes_are_edit_saves) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "A\nbody"), server_note(2, "B\nbody")});
    auto id1 = store->get_by_remote_id(1)->local_id;
    auto id2 = store->get_by_remote_id(2)->local_id;
    PB_CHECK(store->set_favorite(id1, true, 3000).error == EditError::None);
    PB_CHECK(store->get(id1)->favorite);
    PB_CHECK(store->get(id1)->status == NoteStatus::Edited);
    PB_CHECK_EQ(store->get(id1)->modified, static_cast<int64_t>(3000));
    PB_CHECK(store->set_category(id2, "Ideas", 3100).error == EditError::None);
    PB_CHECK_EQ(store->get(id2)->category, std::string("Ideas"));
    PB_CHECK(store->get(id2)->status == NoteStatus::Edited);
    PB_CHECK(store->set_favorite(id1, true, 3200).error == EditError::None);  // no change: unchanged modified
    PB_CHECK_EQ(store->get(id1)->modified, static_cast<int64_t>(3000));
}

PB_TEST(deleting_a_never_synced_note_removes_the_row) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto id = store->create_local("Draft", "", false, 1000).local_id;
    PB_CHECK(store->mark_deleted(id).error == EditError::None);
    PB_CHECK(!store->get(id).has_value());
    PB_CHECK(store->list().empty());
}

PB_TEST(deleting_a_synced_note_keeps_a_tombstone_hidden_from_the_list) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Synced\nbody")});
    auto id = store->get_by_remote_id(1)->local_id;
    PB_CHECK(store->mark_deleted(id).error == EditError::None);
    auto n = store->get(id);
    PB_CHECK(n.has_value() && n->status == NoteStatus::Deleted);
    PB_CHECK(store->list().empty());
    PB_CHECK(store->mark_deleted(id).error == EditError::None);  // idempotent
    PB_CHECK(store->save_local(id, "revive?", "", false, 9).error == EditError::NotFound);
}

PB_TEST(unknown_ids_are_reported) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    PB_CHECK(store->save_local(99, "x", "", false, 1).error == EditError::NotFound);
    PB_CHECK(store->mark_deleted(99).error == EditError::NotFound);
    PB_CHECK(store->set_favorite(99, true, 1).error == EditError::NotFound);
}

PB_TEST(pending_changes_survive_closing_and_reopening) {
    pbtest::TmpDir d;
    int64_t created = 0, edited = 0, deleted = 0;
    {
        auto store = NoteStore::open(d.file("db"));
        store->apply_remote({server_note(1, "To edit\nbody"), server_note(2, "To delete\nbody")});
        created = store->create_local("Created offline", "Work", false, 1000).local_id;
        edited = store->get_by_remote_id(1)->local_id;
        store->save_local(edited, "Edited offline\nbody", "", true, 1100);
        deleted = store->get_by_remote_id(2)->local_id;
        store->mark_deleted(deleted);
    }
    auto store = NoteStore::open(d.file("db"));
    PB_CHECK(store->get(created)->status == NoteStatus::Edited);
    PB_CHECK_EQ(store->get(edited)->content, std::string("Edited offline\nbody"));
    PB_CHECK(store->get(edited)->favorite);
    PB_CHECK(store->get(deleted)->status == NoteStatus::Deleted);
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(2));
}

PB_TEST_MAIN()
