// US4 (spec 002, data-model.md "Schema change: version 1 -> 2"): the attachments table arrives through the
// existing migration path (backup, integrity check, one-way) and its rows follow their note.
#include <sqlite3.h>
#include <sys/stat.h>

#include <string>

#include "pbnotes/note_store.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::ApiNote;
using pbnotes::NoteStore;

namespace {

// The schema as the MVP (version 1) wrote it.
const char* kV1Schema = R"SQL(
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
INSERT INTO account(id,server_url,username,password,api_versions) VALUES(1,'https://cloud.example.com','alice','pw','1.0, 1.3');
INSERT INTO notes(remote_id,status,title,content,category,favorite,modified,etag,excerpt)
    VALUES(5,'void','Old note','Old note' || char(10) || 'body','Home',0,100,'etag5','body');
INSERT INTO settings(key,value) VALUES('font_size','large');
)SQL";

void make_db(const std::string& path, const char* schema, int version) {
    sqlite3* db = nullptr;
    PB_CHECK(sqlite3_open(path.c_str(), &db) == SQLITE_OK);
    char* err = nullptr;
    PB_CHECK(sqlite3_exec(db, schema, nullptr, nullptr, &err) == SQLITE_OK);
    PB_CHECK(sqlite3_exec(db, ("PRAGMA user_version=" + std::to_string(version)).c_str(), nullptr, nullptr, &err) == SQLITE_OK);
    sqlite3_close(db);
}

// Number of rows of `table`, or -1 if the table does not exist (a separate read-only connection).
int count_rows(const std::string& path, const std::string& table) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) return -2;
    sqlite3_stmt* st = nullptr;
    int n = -1;
    if (sqlite3_prepare_v2(db, ("SELECT COUNT(*) FROM " + table).c_str(), -1, &st, nullptr) == SQLITE_OK) {
        if (sqlite3_step(st) == SQLITE_ROW) n = sqlite3_column_int(st, 0);
    }
    sqlite3_finalize(st);
    sqlite3_close(db);
    return n;
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

ApiNote server_note(int64_t id, const std::string& content) {
    ApiNote n;
    n.id = id;
    n.etag = "e" + std::to_string(id);
    n.content = content;
    n.modified = 100;
    return n;
}

}  // namespace

PB_TEST(the_schema_version_is_at_least_two) { PB_CHECK(NoteStore::kSchemaVersion >= 2); }

PB_TEST(a_version_one_database_opens_as_version_two_with_its_data_intact) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV1Schema, 1);
    {
        std::string error;
        auto store = NoteStore::open(d.file("pbnotes.db"), &error);
        PB_CHECK(store != nullptr);
        PB_CHECK_EQ(store->schema_version(), NoteStore::kSchemaVersion);
        auto account = store->get_account();
        PB_CHECK(account.has_value());
        PB_CHECK_EQ(account->username, std::string("alice"));
        PB_CHECK_EQ(account->api_versions, std::string("1.0, 1.3"));
        PB_CHECK_EQ(store->get_setting("font_size"), std::string("large"));
        auto note = store->get_by_remote_id(5);
        PB_CHECK(note.has_value());
        PB_CHECK_EQ(note->content, std::string("Old note\nbody"));
        PB_CHECK_EQ(note->category, std::string("Home"));
    }
    PB_CHECK_EQ(count_rows(d.file("pbnotes.db"), "attachments"), 0);   // the table exists and is empty
    PB_CHECK_EQ(count_rows(d.file("pbnotes.db"), "notes"), 1);
}

PB_TEST(the_backup_is_made_before_migrating_and_deleted_after_the_integrity_check) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV1Schema, 1);
    bool backup_seen = false;
    int version_in_backup = 0;
    NoteStore::OpenOptions options;
    options.after_backup_hook = [&](const std::string& backup) {
        backup_seen = exists(backup);
        version_in_backup = user_version(backup);
        return true;
    };
    auto store = NoteStore::open(d.file("pbnotes.db"), nullptr, options);
    PB_CHECK(store != nullptr);
    PB_CHECK(backup_seen);
    PB_CHECK_EQ(version_in_backup, 1);
    PB_CHECK(!exists(d.file("pbnotes.db.bak")));   // no lingering copy of the password
}

PB_TEST(a_failing_migration_leaves_the_original_database_untouched) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV1Schema, 1);
    NoteStore::OpenOptions options;
    options.after_backup_hook = [](const std::string&) { return false; };
    std::string error;
    auto store = NoteStore::open(d.file("pbnotes.db"), &error, options);
    PB_CHECK(store == nullptr);
    PB_CHECK(!error.empty());
    PB_CHECK_EQ(user_version(d.file("pbnotes.db")), 1);
    PB_CHECK_EQ(count_rows(d.file("pbnotes.db"), "attachments"), -1);   // the table was not created
    PB_CHECK_EQ(count_rows(d.file("pbnotes.db"), "notes"), 1);
}

PB_TEST(a_newer_database_is_refused) {
    pbtest::TmpDir d;
    make_db(d.file("pbnotes.db"), kV1Schema, NoteStore::kSchemaVersion + 1);
    std::string error;
    PB_CHECK(NoteStore::open(d.file("pbnotes.db"), &error) == nullptr);
    PB_CHECK(error.find("newer") != std::string::npos);
    PB_CHECK_EQ(user_version(d.file("pbnotes.db")), NoteStore::kSchemaVersion + 1);
}

PB_TEST(a_fresh_database_has_the_current_version_and_the_attachments_table) {
    pbtest::TmpDir d;
    { auto store = NoteStore::open(d.file("pbnotes.db")); PB_CHECK(store != nullptr); PB_CHECK_EQ(store->schema_version(), NoteStore::kSchemaVersion); }
    PB_CHECK_EQ(count_rows(d.file("pbnotes.db"), "attachments"), 0);
}

PB_TEST(attachment_rows_follow_their_note_when_it_is_removed) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "A\n![x](.attachments.1/a.png)"), server_note(2, "B"), server_note(3, "C")});
    const int64_t a = store->get_by_remote_id(1)->local_id, b = store->get_by_remote_id(2)->local_id;
    PB_CHECK(store->record_downloaded(a, ".attachments.1/a.png", 10, "1.png"));
    PB_CHECK(store->record_downloaded(b, ".attachments.2/b.png", 10, "2.png"));
    const int64_t row_a = store->attachment_for(a, ".attachments.1/a.png")->id;
    const int64_t row_b = store->attachment_for(b, ".attachments.2/b.png")->id;

    // a tombstone removed after the server confirmed the delete
    PB_CHECK(store->mark_deleted(a).error == pbnotes::EditError::None);
    PB_CHECK(store->get_attachment(row_a).has_value());     // the tombstone still holds it
    PB_CHECK(store->remove_note(a));
    PB_CHECK(!store->get_attachment(row_a).has_value());
    PB_CHECK(store->get_attachment(row_b).has_value());

    // a note the server no longer has (pull deletion)
    store->apply_remote({server_note(3, "C")});
    PB_CHECK(!store->get_attachment(row_b).has_value());
}

PB_TEST(reset_local_data_removes_the_attachment_rows) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("db"));
    store->apply_remote({server_note(1, "A")});
    const int64_t a = store->get_by_remote_id(1)->local_id;
    PB_CHECK(store->record_downloaded(a, "x.png", 10, "1.png"));
    const int64_t row = store->attachment_for(a, "x.png")->id;
    PB_CHECK(store->reset_local_data());
    PB_CHECK(!store->get_attachment(row).has_value());
    PB_CHECK(store->referenced_local_files().empty());
}

PB_TEST_MAIN()
