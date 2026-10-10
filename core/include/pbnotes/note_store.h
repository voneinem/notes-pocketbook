#pragma once
// SQLite-backed local store (data-model.md). One connection guarded by one mutex. No Qt.

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "pbnotes/types.h"

struct sqlite3;

namespace pbnotes {

// One row of the note list (no content).
struct NoteSummary {
    int64_t local_id = 0;
    std::string title;
    std::string category;
    std::string excerpt;
    bool favorite = false;
    int64_t modified = 0;
    bool pending = false;  // has local changes the server has not confirmed yet
};

enum class EditError { None, EmptyContent, InvalidCategory, NotFound, StorageError };

struct EditResult {
    EditError error = EditError::None;
    int64_t local_id = 0;
    std::string error_key;  // content_empty | category_* | not_found | storage_error (empty on success)
};

// Category rules (data-model.md): at most 255 characters, no leading or trailing space, no control
// characters (U+0000-U+001F, U+007F), valid UTF-8; "/" is allowed and the empty string means
// "uncategorized". Returns "" if valid, else category_too_long | category_spaces |
// category_control_chars | category_invalid.
std::string validate_category(const std::string& category);

// What the note list shows: a view, a sort order and an optional search text.
//   view:   "all" | "favorites" | "uncategorized" | "category:<name>" (anything else = "all")
//   sort:   "modified" (newest first) | "title" (alphabetical, case-insensitive; anything else = "modified")
//   search: case-insensitive substring of title or content; blank = no filter
struct ListQuery {
    std::string view = "all";
    std::string sort = "modified";
    std::string search;
};

struct CategoryCount {
    std::string name;
    int count = 0;
};

struct ConflictResolution {
    bool ok = false;
    bool created_copy = false;      // false when both versions were identical (nothing to keep apart)
    int64_t copy_local_id = 0;
};

struct PullApplyResult {
    int inserted = 0;
    int updated = 0;
    int removed = 0;
    std::vector<int64_t> need_full_fetch;  // pruned entries for notes we do not have locally
};

class NoteStore {
public:
    static constexpr int kSchemaVersion = 1;

    struct OpenOptions {
        // Test hook: called after the pre-migration backup was written, with the backup path;
        // returning false simulates a failing migration.
        std::function<bool(const std::string& backup_path)> after_backup_hook;
    };

    // Opens (and creates/migrates) the database at `path`. Returns nullptr and fills `error`
    // when the file is unusable, corrupt, newer than this build, or a migration failed.
    // Pragmas: journal_mode=TRUNCATE (FAT storage), synchronous=FULL, foreign_keys=ON,
    // secure_delete=ON, busy_timeout=5000. Before migrating an existing database a backup
    // `<path>.bak` is made; it is deleted again once the migration and an integrity check
    // succeeded (it would otherwise hold a second plaintext copy of the account password).
    static std::unique_ptr<NoteStore> open(const std::string& path, std::string* error = nullptr,
                                           const OpenOptions& options = OpenOptions());

    ~NoteStore();
    NoteStore(const NoteStore&) = delete;
    NoteStore& operator=(const NoteStore&) = delete;

    const std::string& path() const { return path_; }
    int schema_version() const;
    // Value of `PRAGMA <name>` (diagnostics and tests).
    std::string pragma(const std::string& name) const;

    // Settings: font_size {small,medium,large}, sort_order {modified,title},
    // view {all,favorites,uncategorized,category:<name>}, rotation {auto,portrait,landscape}.
    // get returns the default for unset keys and "" for unknown keys; set rejects unknown keys
    // and invalid values (returns false).
    std::string get_setting(const std::string& key) const;
    bool set_setting(const std::string& key, const std::string& value);

    // The single server account (row id 1). save_account writes exactly the fields given (create or
    // replace); callers pass the stored bookkeeping along when only credentials change.
    // forget_account overwrites the stored credentials before deleting the row; with
    // secure_delete=ON no trace of the password stays in the database file (tested).
    bool save_account(const Account& account);
    std::optional<Account> get_account() const;
    bool forget_account();

    // What the user typed on the setup screen, kept so a failed test does not mean retyping a long
    // URL and app password. Stored like the account (unencrypted, see README); an all-empty draft is
    // not stored. clear_setup_draft overwrites before deleting; forget_account clears it as well.
    struct SetupDraft {
        std::string url, username, password;
    };
    bool save_setup_draft(const SetupDraft& draft);
    std::optional<SetupDraft> get_setup_draft() const;
    bool clear_setup_draft();
    int account_count() const;

    // ---- notes -----------------------------------------------------------------------------
    std::optional<Note> get(int64_t local_id) const;
    std::optional<Note> get_by_remote_id(int64_t remote_id) const;
    // Notes except locally deleted ones, filtered and sorted by `query` (default: all, newest first).
    std::vector<NoteSummary> list(const ListQuery& query = ListQuery()) const;
    // Non-empty categories with their note counts (deleted notes excluded), sorted by name.
    std::vector<CategoryCount> categories() const;

    // Applies a (full) server listing in one transaction:
    //  - unknown remote id: inserted as `void` (a pruned entry for an unknown id is reported in
    //    need_full_fetch instead);
    //  - known, status `void`: updated only if content, category, favorite, modified or etag differ;
    //  - known, status `edited`/`deleted`: left untouched (local changes win until pushed);
    //  - `void` notes with a remote id that are absent from the listing: removed;
    //    pending notes and notes that never reached the server are kept.
    // The title is always derived from the content; the server's title is not stored.
    // True if stored synced notes may carry a title that is not the server's (stored by an older
    // version, or a note with neither text nor title): the next sync then pulls the full list once
    // and calls mark_titles_refreshed() afterwards.
    bool titles_need_refresh() const;
    bool mark_titles_refreshed();
    PullApplyResult apply_remote(const std::vector<ApiNote>& listing);

    // ---- local edits (work offline; every change leaves the note pending until pushed) ------------
    // create_local: content must contain more than whitespace (nothing is stored otherwise).
    // save_local / set_favorite / set_category: mark the note `edited` with a fresh modified time and
    // recompute title and excerpt; saving identical values changes nothing. Whitespace-only content
    // is rejected, as is an invalid category (the previous value stays). A failed write leaves the
    // previous version untouched and reports StorageError.
    // mark_deleted: a note that never reached the server disappears at once; a synced note is
    // kept as a hidden tombstone (`deleted`) until the server delete succeeds.
    EditResult create_local(const std::string& content, const std::string& category, bool favorite, int64_t now);
    EditResult save_local(int64_t local_id, const std::string& content, const std::string& category, bool favorite,
                          int64_t now);
    EditResult set_favorite(int64_t local_id, bool favorite, int64_t now);
    EditResult set_category(int64_t local_id, const std::string& category, int64_t now);
    EditResult mark_deleted(int64_t local_id);
    // Flips one task-list checkbox (see toggle_task_item) as a normal edit; a line that is not a task
    // item changes nothing.
    EditResult toggle_task(int64_t local_id, int line_index, int64_t now);

    // Removes all local notes (used when the account is forgotten). Refused (returns false, nothing
    // deleted) while any note has unsynced changes, so nothing the user wrote can be lost silently.
    bool reset_local_data();

    // Runs raw SQL; diagnostics and tests only (for example to simulate a full disk).
    bool debug_exec(const std::string& sql);

    // ---- push support (used by the sync engine) --------------------------------------------------
    // Notes with unconfirmed local changes (`edited` / `deleted`), oldest local id first.
    std::vector<Note> pending_notes() const;
    // What was sent to the server for a note.
    struct PushSnapshot {
        std::string content, category;
        bool favorite = false;
    };
    // Stores the server's answer to a successful POST/PUT in one transaction. The remote id (after a
    // POST) and the validator are always stored. The note becomes `void` with the server's modified
    // time ONLY if it still holds exactly the values that were sent; if the user changed it while the
    // request was in flight it stays `edited` with the newer local values (constitution: a local
    // change is cleared only when the server has confirmed exactly it).
    bool confirm_push(int64_t local_id, const ApiNote& server, const PushSnapshot& sent);
    // The server no longer has the note: forget its identity so the next push creates it again.
    bool clear_remote_identity(int64_t local_id);
    // Removes a tombstone after the server confirmed the delete (only rows with status `deleted`).
    bool remove_note(int64_t local_id);

    // ---- conflicts (constitution Principle VI: never lose a version) ------------------------------
    // The note `local_id` (status `edited`) was changed on the server too. In ONE transaction:
    //  - identical content, category and favorite: the note simply adopts the server's validator and
    //    modified time and becomes `void` (no copy);
    //  - otherwise a conflict copy is inserted FIRST (the local text, with " (conflict YYYY-MM-DD)"
    //    appended to its title line, numbered " #n" if that title exists; status `edited`, no remote
    //    identity), verified against the local text, and only then the original is replaced by the
    //    server version (status `void`).
    // Any failure rolls everything back, so the local text is never held in zero rows. Fails
    // (ok=false) for notes that are not `edited` or unknown.
    ConflictResolution resolve_conflict(int64_t local_id, const ApiNote& server, int64_t now);
    // A locally deleted note was changed on the server: it comes back as the server's version (`void`).
    bool restore_from_server(int64_t local_id, const ApiNote& server);

    // Test hook: makes the n-th (1-based) INSERT/UPDATE/DELETE statement prepared from now on fail;
    // 0 switches the injection off. Used to prove that multi-statement operations are atomic.
    void test_fail_nth_write(int n);

    // ---- sync bookkeeping (account row) ------------------------------------------------------
    bool set_collection_state(const std::optional<std::string>& etag, int64_t last_modified,
                              const std::string& api_versions);
    bool record_sync_result(const SyncResult& result, int64_t now);

private:
    NoteStore() = default;
    EditResult update_locked(int64_t local_id, const std::optional<std::string>& content,
                             const std::optional<std::string>& category, const std::optional<bool>& favorite,
                             int64_t now);

    sqlite3* db_ = nullptr;
    int fail_nth_write_ = 0;
    int write_counter_ = 0;
    static int fault_authorizer(void* user, int action, const char*, const char*, const char*, const char*);
    std::string path_;
    mutable std::mutex mutex_;
};

}  // namespace pbnotes
