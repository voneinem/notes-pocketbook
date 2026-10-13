// T058: local create / save / delete — states, titles, validation, persistence.
#include <string>

#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
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
        auto r = store->create_local(pbnotes::derive_title(empty), empty, "", false, 1000);
        PB_CHECK(r.error == EditError::EmptyContent);
    }
    PB_CHECK(store->list().empty());  // nothing was stored (FR-020)
}

PB_TEST(create_local_makes_a_pending_note_without_remote_identity) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto r = store->create_local(pbnotes::derive_title("Groceries\n- milk\n- eggs"), "Groceries\n- milk\n- eggs", "Home", true, 1000);
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
    auto r = store->save_local(id, pbnotes::derive_title("New title\nnew body"), "New title\nnew body", "Work", false, 2000);
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
    PB_CHECK(store->save_local(id, pbnotes::derive_title("Same\nbody"), "Same\nbody", "", false, 5000).error == EditError::None);
    auto n = store->get(id);
    PB_CHECK(n->status == NoteStatus::Void);  // nothing to push
    PB_CHECK_EQ(n->modified, static_cast<int64_t>(100));
}

PB_TEST(saving_empty_content_is_rejected_and_keeps_the_note) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Keep me\nbody")});
    auto id = store->get_by_remote_id(1)->local_id;
    PB_CHECK(store->save_local(id, "Keep me", "  \n ", "", false, 2000).error == EditError::EmptyContent);
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
    auto id = store->create_local(pbnotes::derive_title("Draft"), "Draft", "", false, 1000).local_id;
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
    PB_CHECK(store->save_local(id, pbnotes::derive_title("revive?"), "revive?", "", false, 9).error == EditError::NotFound);
}

PB_TEST(unknown_ids_are_reported) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    PB_CHECK(store->save_local(99, pbnotes::derive_title("x"), "x", "", false, 1).error == EditError::NotFound);
    PB_CHECK(store->mark_deleted(99).error == EditError::NotFound);
    PB_CHECK(store->set_favorite(99, true, 1).error == EditError::NotFound);
}

PB_TEST(pending_changes_survive_closing_and_reopening) {
    pbtest::TmpDir d;
    int64_t created = 0, edited = 0, deleted = 0;
    {
        auto store = NoteStore::open(d.file("db"));
        store->apply_remote({server_note(1, "To edit\nbody"), server_note(2, "To delete\nbody")});
        created = store->create_local(pbnotes::derive_title("Created offline"), "Created offline", "Work", false, 1000).local_id;
        edited = store->get_by_remote_id(1)->local_id;
        store->save_local(edited, pbnotes::derive_title("Edited offline\nbody"), "Edited offline\nbody", "", true, 1100);
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

// ---- spec 004: explicit titles ---------------------------------------------------------------------------------------

PB_TEST(a_new_note_stores_the_title_the_user_entered_trimmed) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto r = store->create_local("  Groceries  ", "Buy milk\nand eggs", "", false, 1000);
    PB_CHECK(r.error == EditError::None);
    auto n = store->get(r.local_id);
    PB_CHECK_EQ(n->title, std::string("Groceries"));
    PB_CHECK_EQ(n->content, std::string("Buy milk\nand eggs"));
    PB_CHECK_EQ(n->excerpt, std::string("Buy milk and eggs"));  // FR-023: the first line is not the title, so it stays
}

PB_TEST(a_first_line_that_repeats_the_title_is_left_out_of_the_excerpt) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto id = store->create_local("Groceries", "# Groceries\n- milk", "", false, 1000).local_id;
    PB_CHECK_EQ(store->get(id)->excerpt, std::string("- milk"));
}

PB_TEST(editing_the_text_never_changes_the_title) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto id = store->create_local("Groceries", "First line\nbody", "", false, 1000).local_id;
    PB_CHECK(store->save_local(id, "Groceries", "Completely different\nbody", "", false, 2000).error == EditError::None);
    PB_CHECK_EQ(store->get(id)->title, std::string("Groceries"));
    PB_CHECK_EQ(store->get(id)->content, std::string("Completely different\nbody"));
}

PB_TEST(editing_only_the_title_marks_the_note_edited_and_keeps_the_text_byte_for_byte) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Old title\r\nbody  \n")});
    auto id = store->get_by_remote_id(1)->local_id;
    PB_CHECK(store->save_local(id, "New title", "Old title\r\nbody  \n", "", false, 2000).error == EditError::None);
    auto n = store->get(id);
    PB_CHECK(n->status == NoteStatus::Edited);
    PB_CHECK_EQ(n->title, std::string("New title"));
    PB_CHECK_EQ(n->content, std::string("Old title\r\nbody  \n"));
    PB_CHECK_EQ(n->modified, static_cast<int64_t>(2000));
}

PB_TEST(saving_identical_title_and_text_changes_nothing) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Same\nbody")});
    auto id = store->get_by_remote_id(1)->local_id;
    PB_CHECK(store->save_local(id, "Same", "Same\nbody", "", false, 5000).error == EditError::None);
    PB_CHECK(store->get(id)->status == NoteStatus::Void);
    PB_CHECK(store->save_local(id, "  Same  ", "Same\nbody", "", false, 5000).error == EditError::None);  // trims to the same
    PB_CHECK(store->get(id)->status == NoteStatus::Void);
}

PB_TEST(category_favorite_and_checkbox_edits_keep_the_stored_title) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto id = store->create_local("My title", "- [ ] one\n- [ ] two", "", false, 1000).local_id;
    PB_CHECK(store->set_category(id, "Home", 1100).error == EditError::None);
    PB_CHECK_EQ(store->get(id)->title, std::string("My title"));
    PB_CHECK(store->set_favorite(id, true, 1200).error == EditError::None);
    PB_CHECK_EQ(store->get(id)->title, std::string("My title"));
    PB_CHECK(store->toggle_task(id, 0, 1300).error == EditError::None);
    PB_CHECK_EQ(store->get(id)->title, std::string("My title"));
    PB_CHECK_EQ(store->get(id)->content, std::string("- [x] one\n- [ ] two"));
}

PB_TEST(a_refused_title_stores_nothing_and_names_the_rule) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Keep\nbody")});
    auto id = store->get_by_remote_id(1)->local_id;
    const struct { const char* title; const char* key; } cases[] = {
        {"", "title_empty"}, {"   ", "title_empty"}, {"a\nb", "title_control_chars"}, {"\xC3", "title_invalid"}};
    for (const auto& c : cases) {
        auto r = store->save_local(id, c.title, "Changed text", "Work", true, 2000);
        PB_CHECK(r.error == EditError::InvalidTitle);
        PB_CHECK_EQ(r.error_key, std::string(c.key));
        auto c2 = store->create_local(c.title, "text", "", false, 2000);
        PB_CHECK(c2.error == EditError::InvalidTitle);
    }
    auto too_long = store->save_local(id, std::string(256, 'a'), "Changed text", "", false, 2000);
    PB_CHECK(too_long.error == EditError::InvalidTitle);
    PB_CHECK_EQ(too_long.error_key, std::string("title_too_long"));
    auto n = store->get(id);
    PB_CHECK_EQ(n->title, std::string("Keep"));
    PB_CHECK_EQ(n->content, std::string("Keep\nbody"));
    PB_CHECK(n->status == NoteStatus::Void);
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(1));  // no note was created either
}

PB_TEST(checks_run_in_the_order_title_then_content_then_category) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto both = store->create_local("", "text", " bad category ", false, 1);
    PB_CHECK(both.error == EditError::InvalidTitle);
    auto content_and_category = store->create_local("Title", "   ", " bad category ", false, 1);
    PB_CHECK(content_and_category.error == EditError::EmptyContent);
    auto category = store->create_local("Title", "text", " bad category ", false, 1);
    PB_CHECK(category.error == EditError::InvalidCategory);
}

PB_TEST(an_empty_new_note_is_still_never_stored_even_with_a_title_missing) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto r = store->create_local("", "  \n ", "", false, 1);   // neither title nor text: nothing to say about the title
    PB_CHECK(r.error == EditError::EmptyContent);
    PB_CHECK(store->list().empty());
    auto only_title = store->create_local("Title only", "   ", "", false, 1);
    PB_CHECK(only_title.error == EditError::EmptyContent);   // a title cannot stand in for the text (existing FR-020)
    PB_CHECK(store->list().empty());
}

// ---- Spec 008: the editor's save must not overwrite a version a sync pulled while the editor was open -------------

namespace {
// A pull that replaces an unchanged note with a newer server version (new ETag, new text).
void pulled(NoteStore& store, int64_t remote_id, const std::string& etag, const std::string& content) {
    ApiNote n = server_note(remote_id, content);
    n.etag = etag;
    n.modified = 500;
    store.apply_remote({n});
}
pbnotes::EditBase base_of(const char* content, const char* category = "", bool favorite = false) {
    pbnotes::EditBase b;
    b.content = content;
    b.category = category;
    b.favorite = favorite;
    return b;
}
}  // namespace

PB_TEST(a_guarded_save_of_the_text_the_editor_loaded_is_an_ordinary_save) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Old\nbody")});
    const auto id = store->get_by_remote_id(1)->local_id;
    auto r = store->save_local_if_unchanged(id, base_of("Old\nbody"), "Old", "Old\nedited", "Work", true, 2000);
    PB_CHECK(r.error == EditError::None);
    auto n = store->get(id);
    PB_CHECK(n->status == NoteStatus::Edited);
    PB_CHECK_EQ(n->content, std::string("Old\nedited"));
    PB_CHECK_EQ(n->category, std::string("Work"));
    PB_CHECK_EQ(n->etag.value_or(""), std::string("e1"));
}

PB_TEST(a_guarded_save_after_a_pull_replaced_the_text_writes_nothing) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Old\nbody")});
    const auto id = store->get_by_remote_id(1)->local_id;
    pulled(*store, 1, "e1-newer", "Old\nbody changed elsewhere");
    auto r = store->save_local_if_unchanged(id, base_of("Old\nbody"), "Old", "Old\nmy older text", "", false, 2000);
    PB_CHECK(r.error == EditError::ChangedOnServer);
    PB_CHECK_EQ(r.error_key, std::string("changed_on_server"));
    auto n = store->get(id);
    PB_CHECK(n->status == NoteStatus::Void);                                  // untouched: still the server's version
    PB_CHECK_EQ(n->content, std::string("Old\nbody changed elsewhere"));
    PB_CHECK_EQ(n->etag.value_or(""), std::string("e1-newer"));
    PB_CHECK_EQ(n->modified, static_cast<int64_t>(500));
}

PB_TEST(a_guarded_save_after_a_pull_changed_only_the_category_or_favorite_is_also_refused) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    ApiNote n = server_note(1, "Old\nbody");
    store->apply_remote({n});
    const auto id = store->get_by_remote_id(1)->local_id;
    n.etag = "e1b";
    n.category = "Moved";
    n.favorite = true;
    store->apply_remote({n});
    PB_CHECK(store->save_local_if_unchanged(id, base_of("Old\nbody"), "Old", "Old\nedited", "", false, 2000).error ==
             EditError::ChangedOnServer);
}

PB_TEST(a_guarded_save_after_only_the_etag_changed_is_an_ordinary_save) {
    // the reader's own push was confirmed while the editor was open: same text, new validator
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Old\nbody")});
    const auto id = store->get_by_remote_id(1)->local_id;
    pulled(*store, 1, "e1-after-my-push", "Old\nbody");
    PB_CHECK(store->save_local_if_unchanged(id, base_of("Old\nbody"), "Old", "Old\nedited", "", false, 2000).error == EditError::None);
    PB_CHECK_EQ(store->get(id)->content, std::string("Old\nedited"));
}

PB_TEST(a_guarded_save_of_a_local_only_note_and_of_an_unknown_note) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    const auto id = store->create_local("Local", "Local\ntext", "", false, 1000).local_id;
    PB_CHECK(store->save_local_if_unchanged(id, base_of("Local\ntext"), "Local", "Local\nmore", "", false, 2000).error == EditError::None);
    PB_CHECK(store->save_local_if_unchanged(99999, base_of("x"), "x", "x", "", false, 2000).error == EditError::NotFound);
}

PB_TEST(an_edit_saved_as_a_conflict_copy_leaves_the_original_alone_and_keeps_every_word) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Plan\nserver text")});
    const auto id = store->get_by_remote_id(1)->local_id;
    const int64_t now = 1760227200;   // 2025-10-12
    auto r = store->save_edit_as_conflict_copy(id, "Plan", "Plan\nmy text", "Work", true, now);
    PB_CHECK(r.error == EditError::None);
    PB_CHECK(r.local_id != id);
    auto copy = store->get(r.local_id);
    PB_CHECK(copy.has_value());
    PB_CHECK(copy->status == NoteStatus::Edited);
    PB_CHECK(!copy->remote_id.has_value());
    PB_CHECK_EQ(copy->title, std::string("Plan") + pbnotes::conflict_suffix(now, 1));
    PB_CHECK_EQ(copy->content, std::string("Plan\nmy text"));
    PB_CHECK_EQ(copy->category, std::string("Work"));
    PB_CHECK(copy->favorite);
    auto original = store->get(id);
    PB_CHECK(original->status == NoteStatus::Void);
    PB_CHECK_EQ(original->content, std::string("Plan\nserver text"));
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(2));
}

PB_TEST(a_second_conflict_copy_with_the_same_title_is_numbered) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Plan\nserver text")});
    const auto id = store->get_by_remote_id(1)->local_id;
    const int64_t now = 1760227200;
    const auto first = store->save_edit_as_conflict_copy(id, "Plan", "Plan\none", "", false, now);
    const auto second = store->save_edit_as_conflict_copy(id, "Plan", "Plan\ntwo", "", false, now);
    PB_CHECK(first.error == EditError::None && second.error == EditError::None);
    PB_CHECK_EQ(store->get(first.local_id)->title, std::string("Plan") + pbnotes::conflict_suffix(now, 1));
    PB_CHECK_EQ(store->get(second.local_id)->title, std::string("Plan") + pbnotes::conflict_suffix(now, 2));
}

PB_TEST(a_conflict_copy_of_an_invalid_edit_is_refused_and_stores_nothing) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "Plan\nserver text")});
    const auto id = store->get_by_remote_id(1)->local_id;
    PB_CHECK(store->save_edit_as_conflict_copy(id, "Plan", "   ", "", false, 1).error == EditError::EmptyContent);
    PB_CHECK(store->save_edit_as_conflict_copy(id, "Plan", "text", " bad ", false, 1).error == EditError::InvalidCategory);
    PB_CHECK(store->save_edit_as_conflict_copy(id, "", "text", "", false, 1).error == EditError::InvalidTitle);
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(1));
}

PB_TEST_MAIN()
