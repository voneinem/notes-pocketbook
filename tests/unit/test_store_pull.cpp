// T046: NoteStore::apply_remote — what a pull may and may not change.
#include <sqlite3.h>

#include <algorithm>
#include <string>
#include <vector>

#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::ApiNote;
using pbnotes::NoteStatus;
using pbnotes::NoteStore;

namespace {

ApiNote api_note(int64_t id, const std::string& content, const std::string& etag = "e1", int64_t modified = 100,
                 const std::string& category = "", bool favorite = false) {
    ApiNote n;
    n.id = id;
    n.content = content;
    n.etag = etag;
    n.modified = modified;
    n.category = category;
    n.favorite = favorite;
    return n;
}
ApiNote pruned(int64_t id) {
    ApiNote n;
    n.id = id;
    n.pruned = true;
    return n;
}
// Test-only: force a local status with raw SQL (the edit API belongs to a later story).
void set_status(const std::string& path, int64_t remote_id, const char* status, const char* new_content = nullptr) {
    sqlite3* db = nullptr;
    sqlite3_open(path.c_str(), &db);
    std::string sql = std::string("UPDATE notes SET status='") + status + "'";
    if (new_content) sql += std::string(", content='") + new_content + "'";
    sql += " WHERE remote_id=" + std::to_string(remote_id);
    sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr);
    sqlite3_close(db);
}

}  // namespace

PB_TEST(unknown_remote_ids_are_inserted_as_void_with_derived_title_and_excerpt) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto r = store->apply_remote({api_note(1, "Shopping list\n- milk\n- bread\n", "ea", 100, "Home", true)});
    PB_CHECK_EQ(r.inserted, 1);
    auto n = store->get_by_remote_id(1);
    PB_CHECK(n.has_value());
    PB_CHECK(n->status == NoteStatus::Void);
    PB_CHECK_EQ(n->title, std::string("Shopping list"));  // derived locally; ApiNote does not even carry the server title
    PB_CHECK_EQ(n->excerpt, std::string("- milk - bread"));
    PB_CHECK_EQ(n->category, std::string("Home"));
    PB_CHECK(n->favorite);
    PB_CHECK_EQ(n->modified, static_cast<int64_t>(100));
    PB_CHECK_EQ(n->etag.value_or(""), std::string("ea"));
    PB_CHECK(n->local_id > 0);
}

PB_TEST(a_void_note_is_updated_only_when_a_field_differs) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({api_note(1, "A\nbody", "e1", 100, "c", false)});
    auto same = store->apply_remote({api_note(1, "A\nbody", "e1", 100, "c", false)});
    PB_CHECK_EQ(same.updated, 0);
    for (int variant = 0; variant < 5; ++variant) {
        ApiNote n = api_note(1, "A\nbody", "e1", 100, "c", false);
        if (variant == 0) n.content = "A\nchanged";
        if (variant == 1) n.category = "other";
        if (variant == 2) n.favorite = true;
        if (variant == 3) n.modified = 200;
        if (variant == 4) n.etag = "e2";
        auto r = store->apply_remote({n});
        if (r.updated != 1) PB_FAIL_AT("variant not applied: " + std::to_string(variant));
        store->apply_remote({api_note(1, "A\nbody", "e1", 100, "c", false)});  // reset
    }
}

PB_TEST(a_synced_note_shows_the_server_title_and_falls_back_to_its_first_line) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    ApiNote empty = api_note(1, "");
    empty.server_title = "Sample Product Guide";
    ApiNote renamed = api_note(2, "C:\\Windows>cmd\nbody");
    renamed.server_title = "Disk Encryption Setup";
    ApiNote untitled = api_note(3, "My own first line\nbody");  // the server gave no title
    store->apply_remote({empty, renamed, untitled});
    std::vector<std::string> titles;
    for (const auto& s : store->list()) titles.push_back(s.title);
    std::sort(titles.begin(), titles.end());
    PB_CHECK_EQ(titles.size(), static_cast<size_t>(3));
    PB_CHECK_EQ(titles[0], std::string("Disk Encryption Setup"));
    PB_CHECK_EQ(titles[1], std::string("My own first line"));
    PB_CHECK_EQ(titles[2], std::string("Sample Product Guide"));
}

PB_TEST(a_local_edit_shows_the_title_the_user_entered_until_the_server_confirms) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    ApiNote n = api_note(1, "Old first line\nbody");
    n.server_title = "Server file name";
    store->apply_remote({n});
    const int64_t id = store->list()[0].local_id;
    PB_CHECK_EQ(store->get(id)->title, std::string("Server file name"));
    PB_CHECK(store->save_local(id, "My own title", "New first line\nbody", "", false, 200).error == pbnotes::EditError::None);
    PB_CHECK_EQ(store->get(id)->title, std::string("My own title"));   // not the first line (spec 004)
    // a pull must not touch the edited note, not even when the server's title changes
    ApiNote again = n;
    again.etag = "e2";
    again.server_title = "Renamed on the server";
    store->apply_remote({again});
    PB_CHECK_EQ(store->get(id)->title, std::string("My own title"));
    PB_CHECK_EQ(store->get(id)->content, std::string("New first line\nbody"));
}

PB_TEST(a_changed_server_title_reaches_a_note_without_local_changes) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    ApiNote n = api_note(1, "Body line\nmore");
    n.server_title = "First name";
    store->apply_remote({n});
    ApiNote renamed = n;
    renamed.etag = "e2";
    renamed.server_title = "Second name";
    auto r = store->apply_remote({renamed});
    PB_CHECK_EQ(r.updated, 1);
    PB_CHECK_EQ(store->get_by_remote_id(1)->title, std::string("Second name"));
    PB_CHECK_EQ(store->get_by_remote_id(1)->content, std::string("Body line\nmore"));
}

PB_TEST(the_excerpt_of_a_pulled_note_keeps_a_first_line_that_is_not_the_title) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    ApiNote renamed = api_note(1, "C:\\Windows>cmd\nbody");
    renamed.server_title = "Disk Encryption Setup";
    ApiNote same = api_note(2, "Plain title\nbody");
    same.server_title = "Plain title";
    store->apply_remote({renamed, same});
    PB_CHECK_EQ(store->get_by_remote_id(1)->excerpt, std::string("C:\\Windows>cmd body"));   // FR-023: nothing dropped
    PB_CHECK_EQ(store->get_by_remote_id(2)->excerpt, std::string("body"));                  // a repeated title is left out
}

PB_TEST(titles_stored_by_an_older_version_are_refreshed_by_one_full_pull) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK(!store->titles_need_refresh());        // nothing stored yet
    store->apply_remote({api_note(1, "Heading as written\nbody")});
    PB_CHECK(store->titles_need_refresh());         // synced notes, no marker yet
    ApiNote again = api_note(1, "Heading as written\nbody");   // same etag, content and fields
    again.server_title = "Server name";
    store->apply_remote({again});                   // only the title is new: it is applied
    PB_CHECK_EQ(store->list()[0].title, std::string("Server name"));
    PB_CHECK(store->mark_titles_refreshed());
    PB_CHECK(!store->titles_need_refresh());
}

PB_TEST(a_stored_empty_note_without_title_is_refreshed_even_after_the_marker) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    store->apply_remote({api_note(1, "")});
    store->mark_titles_refreshed();
    PB_CHECK(store->titles_need_refresh());         // no text and no title
    ApiNote again = api_note(1, "");
    again.server_title = "Sample Registry Notes";
    store->apply_remote({again});
    PB_CHECK(!store->titles_need_refresh());
}

PB_TEST(content_change_recomputes_title_and_excerpt) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({api_note(1, "Old title\nold body")});
    store->apply_remote({api_note(1, "New title\nnew body", "e2")});
    auto n = store->get_by_remote_id(1);
    PB_CHECK_EQ(n->title, std::string("New title"));
    PB_CHECK_EQ(n->excerpt, std::string("new body"));
}

PB_TEST(pending_notes_are_never_touched_by_a_pull) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({api_note(1, "Edited locally"), api_note(2, "Deleted locally")});
    set_status(d.file("db"), 1, "edited", "My local edit");
    set_status(d.file("db"), 2, "deleted");
    auto r = store->apply_remote({api_note(1, "Server version", "e9", 999), api_note(2, "Server version 2", "e8", 999)});
    PB_CHECK_EQ(r.updated, 0);
    auto n1 = store->get_by_remote_id(1);
    PB_CHECK(n1->status == NoteStatus::Edited);
    PB_CHECK_EQ(n1->content, std::string("My local edit"));
    PB_CHECK(store->get_by_remote_id(2)->status == NoteStatus::Deleted);
}

PB_TEST(void_notes_missing_from_the_listing_are_removed_but_pending_ones_survive) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({api_note(1, "one"), api_note(2, "two"), api_note(3, "three"), api_note(4, "four")});
    set_status(d.file("db"), 3, "edited");
    set_status(d.file("db"), 4, "deleted");
    auto r = store->apply_remote({pruned(2)});  // notes 1, 3 and 4 are gone from the server
    PB_CHECK_EQ(r.removed, 1);                  // only the void note 1
    PB_CHECK(!store->get_by_remote_id(1).has_value());
    PB_CHECK(store->get_by_remote_id(2).has_value());
    PB_CHECK(store->get_by_remote_id(3).has_value());  // edited locally: survives (re-created on the next push)
    PB_CHECK(store->get_by_remote_id(4).has_value());  // deleted locally: row kept until the server delete succeeds
}

PB_TEST(local_only_notes_are_not_removed_by_a_pull) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({api_note(1, "synced")});
    // A note that never reached the server has no remote id (raw insert; the edit API is later work).
    sqlite3* db = nullptr;
    sqlite3_open(d.file("db").c_str(), &db);
    sqlite3_exec(db, "INSERT INTO notes(status,title,content,category,favorite,modified,excerpt) "
                     "VALUES('edited','Local','Local',  '',0,5,'')", nullptr, nullptr, nullptr);
    sqlite3_close(db);
    store->apply_remote({});  // the server list is empty
    PB_CHECK(!store->get_by_remote_id(1).has_value());
    PB_CHECK_EQ(store->list().size(), static_cast<size_t>(1));  // the local-only note stays
}

PB_TEST(a_pruned_entry_for_an_unknown_note_must_be_fetched) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    auto r = store->apply_remote({pruned(42)});
    PB_CHECK_EQ(r.inserted, 0);
    PB_CHECK_EQ(r.need_full_fetch.size(), static_cast<size_t>(1));
    PB_CHECK_EQ(r.need_full_fetch[0], static_cast<int64_t>(42));
}

PB_TEST(collection_state_and_sync_result_are_stored_on_the_account) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    pbnotes::Account a;
    a.server_url = "https://cloud.example.com";
    a.username = "alice";
    a.password = "pw";
    PB_CHECK(store->save_account(a));
    PB_CHECK(store->set_collection_state(std::string("\"coll\""), 1700000000, "1.0, 1.3"));
    pbnotes::SyncResult res;
    res.outcome = pbnotes::SyncOutcome::Success;
    res.message_key = "ok";
    res.counts.pulled = 3;
    PB_CHECK(store->record_sync_result(res, 1700001234));
    auto acc = store->get_account();
    PB_CHECK_EQ(acc->collection_etag.value_or(""), std::string("\"coll\""));
    PB_CHECK_EQ(acc->collection_last_modified, static_cast<int64_t>(1700000000));
    PB_CHECK_EQ(acc->api_versions, std::string("1.0, 1.3"));
    PB_CHECK_EQ(acc->last_sync_at.value_or(0), static_cast<int64_t>(1700001234));
    PB_CHECK(acc->last_sync_outcome == pbnotes::SyncOutcome::Success);
    PB_CHECK_EQ(acc->last_sync_counts.pulled, 3);
}

PB_TEST(bookkeeping_needs_an_account) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    PB_CHECK(!store->set_collection_state(std::nullopt, 0, ""));
    PB_CHECK(!store->record_sync_result(pbnotes::SyncResult(), 1));
}

PB_TEST_MAIN()
