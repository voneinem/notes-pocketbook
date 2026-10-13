// Spec 004 (data-model.md "Schema change: version 2 -> 3"): a data-only step through the existing migration path.
// Unsynced notes lose the heading marker of their (formerly derived) title, every preview is recomputed with the
// FR-023 rule, and nothing else changes.
#include <sqlite3.h>
#include <sys/stat.h>

#include <chrono>
#include <string>

#include "pbnotes/note_store.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::NoteStatus;
using pbnotes::NoteStore;

namespace {

// The schema as feature 002 (version 2) wrote it.
const char* kV2Schema = R"SQL(
CREATE TABLE account(
    id INTEGER PRIMARY KEY CHECK(id = 1),
    server_url TEXT NOT NULL, username TEXT NOT NULL, password TEXT NOT NULL,
    collection_etag TEXT, collection_last_modified INTEGER NOT NULL DEFAULT 0,
    api_versions TEXT NOT NULL DEFAULT '', last_sync_at INTEGER,
    last_sync_outcome TEXT NOT NULL DEFAULT 'never'
        CHECK(last_sync_outcome IN ('never','success','partial','failed','offline','maintenance')),
    last_sync_message TEXT NOT NULL DEFAULT '', last_sync_counts TEXT NOT NULL DEFAULT '{}'
);
CREATE TABLE notes(
    local_id INTEGER PRIMARY KEY AUTOINCREMENT, remote_id INTEGER UNIQUE,
    status TEXT NOT NULL CHECK(status IN ('void','edited','deleted')),
    title TEXT NOT NULL, content TEXT NOT NULL, category TEXT NOT NULL DEFAULT '',
    favorite INTEGER NOT NULL DEFAULT 0 CHECK(favorite IN (0,1)), modified INTEGER NOT NULL,
    etag TEXT, excerpt TEXT NOT NULL DEFAULT '',
    CHECK(remote_id IS NULL OR etag IS NOT NULL)
);
CREATE TABLE settings(key TEXT PRIMARY KEY, value TEXT NOT NULL);
CREATE TABLE attachments(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    note_local_id INTEGER REFERENCES notes(local_id) ON DELETE CASCADE,
    remote_path TEXT, display_name TEXT NOT NULL,
    kind TEXT NOT NULL CHECK(kind IN ('image','file')), size INTEGER,
    state TEXT NOT NULL CHECK(state IN ('staged','pending','remote','downloaded')),
    error_key TEXT NOT NULL DEFAULT '', local_file TEXT NOT NULL DEFAULT '', created INTEGER NOT NULL
);
INSERT INTO account(id,server_url,username,password,api_versions) VALUES(1,'https://cloud.example.com','alice','pw','1.0, 1.3');
-- 1: unsynced, derived title with a heading marker; the old preview skipped the first line
INSERT INTO notes(local_id,remote_id,status,title,content,category,favorite,modified,etag,excerpt)
    VALUES(1,NULL,'edited','## Plan','## Plan' || char(10) || 'step one' || char(10) || 'step two','Work',1,300,NULL,'step one step two');
-- 2: synced, the server calls it differently from its first line; the old preview lost that first line
INSERT INTO notes(local_id,remote_id,status,title,content,category,favorite,modified,etag,excerpt)
    VALUES(2,7,'void','# Server name','Heading as written' || char(10) || 'body','Home',0,200,'etag7','body');
-- 3: synced, title equals the first line
INSERT INTO notes(local_id,remote_id,status,title,content,category,favorite,modified,etag,excerpt)
    VALUES(3,8,'void','Groceries','Groceries' || char(10) || 'milk','',0,100,'etag8','milk');
-- 4: unsynced edit of a synced note, title without marker, hashtag title that must stay
INSERT INTO notes(local_id,remote_id,status,title,content,category,favorite,modified,etag,excerpt)
    VALUES(4,9,'edited','#hashtag','#hashtag now' || char(10) || 'x','',0,400,'etag9','x');
-- 6: an unsynced note whose first line was a long paragraph with a control character: the old title was all of it
INSERT INTO notes(local_id,remote_id,status,title,content,category,favorite,modified,etag,excerpt)
    VALUES(6,NULL,'edited',replace(hex(zeroblob(150)),'0','ab') || char(9) || 'tail',replace(hex(zeroblob(150)),'0','ab') || char(9) || 'tail',
           '',0,500,NULL,'');
-- 5: a locally deleted note keeps its row
INSERT INTO notes(local_id,remote_id,status,title,content,category,favorite,modified,etag,excerpt)
    VALUES(5,10,'deleted','### Old','### Old' || char(10) || 'gone','',0,50,'etag10','gone');
)SQL";

void make_db(const std::string& path, const char* schema, int version) {
    sqlite3* db = nullptr;
    PB_CHECK(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
    char* err = nullptr;
    PB_CHECK(sqlite3_exec(db, schema, nullptr, nullptr, &err) == SQLITE_OK);
    PB_CHECK(sqlite3_exec(db, ("PRAGMA user_version=" + std::to_string(version)).c_str(), nullptr, nullptr, &err) == SQLITE_OK);
    sqlite3_close(db);
}

int user_version(const std::string& path) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return -1;
    sqlite3_stmt* st = nullptr;
    int v = -1;
    if (sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &st, nullptr) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW)
        v = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    sqlite3_close(db);
    return v;
}

bool exists(const std::string& path) {
    struct stat s;
    return ::stat(path.c_str(), &s) == 0;
}

// One text column of the row with local_id, read through a separate connection ("" if none).
std::string column(const std::string& path, const char* col, int64_t local_id) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return "?";
    sqlite3_stmt* st = nullptr;
    std::string out;
    const std::string sql = std::string("SELECT ") + col + " FROM notes WHERE local_id=" + std::to_string(local_id);
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char* t = sqlite3_column_text(st, 0);
        if (t) out = reinterpret_cast<const char*>(t);
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return out;
}

}  // namespace

PB_TEST(the_schema_version_is_three) { PB_CHECK_EQ(NoteStore::kSchemaVersion, 3); }

PB_TEST(a_version_two_database_opens_as_version_three) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV2Schema, 2);
    {
        std::string error;
        auto store = NoteStore::open(d.file("pbnotes.db"), &error);
        PB_CHECK(store != nullptr);
        PB_CHECK_EQ(store->schema_version(), 3);
        PB_CHECK_EQ(store->get_setting("sync_after_save"), std::string("on"));   // spec 008: an upgrade starts with Auto Sync on
        PB_CHECK_EQ(store->get_setting("picker_view"), std::string("list"));      // spec 009: and with the file browser as a list
        PB_CHECK_EQ(store->get_setting("list_layout"), std::string("list"));      // spec 010: and the note list as a list
    }
    PB_CHECK_EQ(user_version(d.file("pbnotes.db")), 3);
    PB_CHECK(!exists(d.file("pbnotes.db.bak")));   // no lingering copy of the password
}

PB_TEST(the_heading_marker_is_removed_from_the_titles_of_unsynced_notes_only) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV2Schema, 2);
    { auto store = NoteStore::open(d.file("pbnotes.db")); PB_CHECK(store != nullptr); }
    PB_CHECK_EQ(column(d.file("pbnotes.db"), "title", 1), std::string("Plan"));          // edited: "## Plan" -> "Plan"
    PB_CHECK_EQ(column(d.file("pbnotes.db"), "title", 2), std::string("# Server name")); // void: the server's, untouched
    PB_CHECK_EQ(column(d.file("pbnotes.db"), "title", 3), std::string("Groceries"));
    PB_CHECK_EQ(column(d.file("pbnotes.db"), "title", 4), std::string("#hashtag"));      // not a heading marker
    PB_CHECK_EQ(column(d.file("pbnotes.db"), "title", 5), std::string("### Old"));       // deleted: untouched
}

PB_TEST(a_derived_title_that_breaks_the_title_rules_is_made_valid_so_the_note_can_still_be_pushed) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV2Schema, 2);
    { auto store = NoteStore::open(d.file("pbnotes.db")); PB_CHECK(store != nullptr); }
    auto store = NoteStore::open(d.file("pbnotes.db"));
    const std::string title = store->get(6)->title;
    PB_CHECK(pbnotes::validate_title(title).empty());
    PB_CHECK_EQ(title.size(), static_cast<size_t>(255));
    PB_CHECK_EQ(store->get(6)->content.substr(0, 4), std::string("abab"));   // the text itself is untouched
}

PB_TEST(every_preview_is_recomputed_with_the_rule_that_keeps_a_first_line_which_is_not_the_title) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV2Schema, 2);
    { auto store = NoteStore::open(d.file("pbnotes.db")); PB_CHECK(store != nullptr); }
    PB_CHECK_EQ(column(d.file("pbnotes.db"), "excerpt", 1), std::string("step one step two"));          // "## Plan" repeated "Plan"
    PB_CHECK_EQ(column(d.file("pbnotes.db"), "excerpt", 2), std::string("Heading as written body"));   // line kept now
    PB_CHECK_EQ(column(d.file("pbnotes.db"), "excerpt", 3), std::string("milk"));
    PB_CHECK_EQ(column(d.file("pbnotes.db"), "excerpt", 4), std::string("#hashtag now x"));
}

PB_TEST(nothing_else_of_any_note_is_touched) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV2Schema, 2);
    { auto store = NoteStore::open(d.file("pbnotes.db")); PB_CHECK(store != nullptr); }
    auto store = NoteStore::open(d.file("pbnotes.db"));
    auto n1 = store->get(1);
    PB_CHECK(n1->status == NoteStatus::Edited);
    PB_CHECK(!n1->remote_id.has_value());
    PB_CHECK_EQ(n1->content, std::string("## Plan\nstep one\nstep two"));
    PB_CHECK_EQ(n1->category, std::string("Work"));
    PB_CHECK(n1->favorite);
    PB_CHECK_EQ(n1->modified, static_cast<int64_t>(300));
    auto n2 = store->get(2);
    PB_CHECK(n2->status == NoteStatus::Void);
    PB_CHECK_EQ(n2->etag.value_or(""), std::string("etag7"));
    PB_CHECK_EQ(n2->modified, static_cast<int64_t>(200));
    PB_CHECK_EQ(n2->category, std::string("Home"));
    auto n4 = store->get(4);
    PB_CHECK(n4->status == NoteStatus::Edited);
    PB_CHECK_EQ(n4->remote_id.value_or(0), static_cast<int64_t>(9));
    PB_CHECK_EQ(n4->etag.value_or(""), std::string("etag9"));
    PB_CHECK(store->get(5)->status == NoteStatus::Deleted);
}

PB_TEST(reopening_a_migrated_database_changes_nothing_again) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV2Schema, 2);
    { auto store = NoteStore::open(d.file("pbnotes.db")); }
    // the user renames a note; a reopen must not "repair" the new title
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        PB_CHECK(store->save_local(1, "## Mine", "## Plan\nstep one\nstep two", "Work", true, 999).error == pbnotes::EditError::None);
    }
    auto store = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK_EQ(store->get(1)->title, std::string("## Mine"));   // a title the user typed is kept as typed
}

PB_TEST(a_failing_migration_leaves_the_version_two_database_untouched_and_keeps_the_backup) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV2Schema, 2);
    NoteStore::OpenOptions options;
    options.after_backup_hook = [](const std::string&) { return false; };
    std::string error;
    PB_CHECK(NoteStore::open(d.file("pbnotes.db"), &error, options) == nullptr);
    PB_CHECK(!error.empty());
    PB_CHECK_EQ(user_version(d.file("pbnotes.db")), 2);
    PB_CHECK_EQ(column(d.file("pbnotes.db"), "title", 1), std::string("## Plan"));
    PB_CHECK(exists(d.file("pbnotes.db.bak")));
}

PB_TEST(a_database_from_a_newer_version_is_refused) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV2Schema, 4);
    std::string error;
    PB_CHECK(NoteStore::open(d.file("pbnotes.db"), &error) == nullptr);
    PB_CHECK(error.find("newer") != std::string::npos);
    PB_CHECK_EQ(user_version(d.file("pbnotes.db")), 4);
}

PB_TEST(five_thousand_notes_migrate_in_under_five_seconds) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV2Schema, 2);
    {
        sqlite3* db = nullptr;
        PB_CHECK(sqlite3_open(d.file("pbnotes.db").c_str(), &db) == SQLITE_OK);
        PB_CHECK(sqlite3_exec(db,
                              "WITH RECURSIVE n(i) AS (SELECT 100 UNION ALL SELECT i+1 FROM n WHERE i < 5099) "
                              "INSERT INTO notes(local_id,remote_id,status,title,content,category,favorite,modified,etag,excerpt) "
                              "SELECT i, i, CASE WHEN i % 2 THEN 'edited' ELSE 'void' END, '## Note ' || i, "
                              "'## Note ' || i || char(10) || 'some body text of note ' || i, '', 0, i, 'e' || i, 'x' FROM n",
                              nullptr, nullptr, nullptr) == SQLITE_OK);
        sqlite3_close(db);
    }
    const auto t0 = std::chrono::steady_clock::now();
    auto store = NoteStore::open(d.file("pbnotes.db"));
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    PB_CHECK(store != nullptr);
    PB_CHECK(seconds < 5.0);
    PB_CHECK_EQ(store->get(101)->title, std::string("Note 101"));     // edited -> marker removed
    PB_CHECK_EQ(store->get(102)->title, std::string("## Note 102"));  // void -> untouched
    PB_CHECK_EQ(store->get(102)->excerpt, std::string("some body text of note 102"));
}

PB_TEST_MAIN()
