// T017: NoteStore::open creates the schema with the required pragmas, migrates safely
// (backup first, removed after success) and reports failures instead of ignoring them.
#include <sqlite3.h>

#include <string>

#include "pbnotes/note_store.h"
#include "test_support.h"
#include "tmp_dir.h"

using pbnotes::NoteStore;

PB_TEST(new_database_has_required_pragmas_and_schema_version) {
    pbtest::TmpDir d;
    std::string err;
    auto store = NoteStore::open(d.file("pbnotes.db"), &err);
    PB_CHECK(store != nullptr);
    PB_CHECK_EQ(store->pragma("journal_mode"), std::string("truncate"));
    PB_CHECK_EQ(store->pragma("synchronous"), std::string("2"));  // FULL
    PB_CHECK_EQ(store->pragma("foreign_keys"), std::string("1"));
    PB_CHECK_EQ(store->pragma("secure_delete"), std::string("1"));
    PB_CHECK_EQ(store->pragma("busy_timeout"), std::string("5000"));
    PB_CHECK_EQ(store->schema_version(), NoteStore::kSchemaVersion);
    PB_CHECK_EQ(store->pragma("user_version"), std::to_string(NoteStore::kSchemaVersion));
}

PB_TEST(default_settings) {
    pbtest::TmpDir d;
    auto store = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK(store != nullptr);
    PB_CHECK_EQ(store->get_setting("font_size"), std::string("medium"));
    PB_CHECK_EQ(store->get_setting("sort_order"), std::string("modified"));
    PB_CHECK_EQ(store->get_setting("view"), std::string("all"));
    PB_CHECK_EQ(store->get_setting("rotation"), std::string("auto"));
}

PB_TEST(settings_are_validated_and_persisted) {
    pbtest::TmpDir d;
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        PB_CHECK(store->set_setting("font_size", "large"));
        PB_CHECK(!store->set_setting("font_size", "huge"));
        PB_CHECK(!store->set_setting("sort_order", "random"));
        PB_CHECK(!store->set_setting("rotation", "diagonal"));
        PB_CHECK_EQ(store->get_setting("font_size"), std::string("large"));
    }
    auto again = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK_EQ(again->get_setting("font_size"), std::string("large"));
}

PB_TEST(reopening_a_current_database_makes_no_backup) {
    pbtest::TmpDir d;
    PB_CHECK(NoteStore::open(d.file("pbnotes.db")) != nullptr);
    PB_CHECK(NoteStore::open(d.file("pbnotes.db")) != nullptr);
    PB_CHECK(!d.exists("pbnotes.db.bak"));
}

namespace {
// A database from an older (unversioned) layout: non-empty, user_version 0.
void make_legacy_db(const std::string& path) {
    sqlite3* db = nullptr;
    sqlite3_open(path.c_str(), &db);
    sqlite3_exec(db, "CREATE TABLE legacy(x TEXT); INSERT INTO legacy VALUES('keep me');", nullptr, nullptr, nullptr);
    sqlite3_close(db);
}
}  // namespace

PB_TEST(migration_backs_up_first_and_removes_the_backup_after_success) {
    pbtest::TmpDir d;
    make_legacy_db(d.file("pbnotes.db"));
    bool backup_existed_during_migration = false;
    NoteStore::OpenOptions opts;
    opts.after_backup_hook = [&](const std::string& backup_path) {
        backup_existed_during_migration = d.exists("pbnotes.db.bak") && backup_path == d.file("pbnotes.db.bak");
        return true;
    };
    std::string err;
    auto store = NoteStore::open(d.file("pbnotes.db"), &err, opts);
    PB_CHECK(store != nullptr);
    PB_CHECK(backup_existed_during_migration);
    PB_CHECK(!d.exists("pbnotes.db.bak"));  // the backup must not linger (plaintext password copy)
    PB_CHECK_EQ(store->schema_version(), NoteStore::kSchemaVersion);
}

PB_TEST(failed_migration_is_reported_and_keeps_the_backup) {
    pbtest::TmpDir d;
    make_legacy_db(d.file("pbnotes.db"));
    NoteStore::OpenOptions opts;
    opts.after_backup_hook = [](const std::string&) { return false; };  // simulate a failing migration
    std::string err;
    auto store = NoteStore::open(d.file("pbnotes.db"), &err, opts);
    PB_CHECK(store == nullptr);
    PB_CHECK(!err.empty());
    PB_CHECK(d.exists("pbnotes.db.bak"));
}

PB_TEST(corrupt_database_is_reported_not_ignored) {
    pbtest::TmpDir d;
    d.write("pbnotes.db", std::string(4096, 'x'));
    std::string err;
    auto store = NoteStore::open(d.file("pbnotes.db"), &err);
    PB_CHECK(store == nullptr);
    PB_CHECK(!err.empty());
}

// Spec 008: "Auto Sync" is a persisted on/off choice, on until the reader switches it off.
PB_TEST(sync_after_save_is_on_by_default_and_accepts_only_on_and_off) {
    pbtest::TmpDir d;
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        PB_CHECK(store != nullptr);
        PB_CHECK_EQ(store->get_setting("sync_after_save"), std::string("on"));
        PB_CHECK(store->set_setting("sync_after_save", "off"));
        PB_CHECK_EQ(store->get_setting("sync_after_save"), std::string("off"));
        for (const char* bad : {"yes", "", "ON", "1", "true"}) {
            PB_CHECK(!store->set_setting("sync_after_save", bad));
            PB_CHECK_EQ(store->get_setting("sync_after_save"), std::string("off"));
        }
    }
    auto again = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK_EQ(again->get_setting("sync_after_save"), std::string("off"));
    PB_CHECK(again->set_setting("sync_after_save", "on"));
    PB_CHECK_EQ(again->get_setting("sync_after_save"), std::string("on"));
}

// Spec 009: the file browser's layout (list or tiles) is a persisted choice, a list until the reader switches it.
PB_TEST(picker_view_is_list_by_default_and_accepts_only_list_and_tile) {
    pbtest::TmpDir d;
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        PB_CHECK(store != nullptr);
        PB_CHECK_EQ(store->get_setting("picker_view"), std::string("list"));
        PB_CHECK(store->set_setting("picker_view", "tile"));
        PB_CHECK_EQ(store->get_setting("picker_view"), std::string("tile"));
        for (const char* bad : {"grid", "", "TILE", "1", "tiles"}) {
            PB_CHECK(!store->set_setting("picker_view", bad));
            PB_CHECK_EQ(store->get_setting("picker_view"), std::string("tile"));
        }
    }
    auto again = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK_EQ(again->get_setting("picker_view"), std::string("tile"));   // survives a restart
    PB_CHECK(again->set_setting("picker_view", "list"));
    PB_CHECK_EQ(again->get_setting("picker_view"), std::string("list"));
}

// Spec 010: the note list's layout (list or tiles) is a persisted choice, a list until the reader switches it.
PB_TEST(list_layout_is_list_by_default_and_accepts_only_list_and_tile) {
    pbtest::TmpDir d;
    {
        auto store = NoteStore::open(d.file("pbnotes.db"));
        PB_CHECK(store != nullptr);
        PB_CHECK_EQ(store->get_setting("list_layout"), std::string("list"));
        PB_CHECK(store->set_setting("list_layout", "tile"));
        PB_CHECK_EQ(store->get_setting("list_layout"), std::string("tile"));
        for (const char* bad : {"grid", "", "TILE", "1", "tiles"}) {
            PB_CHECK(!store->set_setting("list_layout", bad));
            PB_CHECK_EQ(store->get_setting("list_layout"), std::string("tile"));
        }
    }
    auto again = NoteStore::open(d.file("pbnotes.db"));
    PB_CHECK_EQ(again->get_setting("list_layout"), std::string("tile"));   // survives a restart
    PB_CHECK(again->set_setting("list_layout", "list"));
    PB_CHECK_EQ(again->get_setting("list_layout"), std::string("list"));
}

PB_TEST_MAIN()
