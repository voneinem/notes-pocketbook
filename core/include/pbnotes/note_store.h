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

enum class EditError { None, EmptyContent, InvalidTitle, InvalidCategory, NotFound, StorageError, ChangedOnServer };

// What the editor showed when it opened a note (spec 008): the text, category and favorite mark. The title is left
// out on purpose: a confirmed upload may rewrite it (the server's unique title) without anyone changing the text.
struct EditBase {
    std::string content;
    std::string category;
    bool favorite = false;
};

struct EditResult {
    EditError error = EditError::None;
    int64_t local_id = 0;
    std::string error_key;  // content_empty | title_* | category_* | not_found | storage_error | changed_on_server (empty on success)
};

// Title rules (spec 004, data-model.md): trimmed of leading and trailing ASCII whitespace (trim_title), then
// non-empty, valid UTF-8, no control characters (U+0000-U+001F, U+007F), at most 255 characters (code points).
// validate_title takes the title as typed and returns "" if it is valid after trimming, else title_empty |
// title_too_long | title_control_chars | title_invalid (checked in the order invalid UTF-8, control
// characters, length).
std::string trim_title(const std::string& title);
std::string validate_title(const std::string& title);

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
    static constexpr int kSchemaVersion = 3;

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
    // view {all,favorites,uncategorized,category:<name>}, rotation {auto,portrait,landscape},
    // sync_after_save {off,on} (default on; the Settings control is called "Auto Sync": a saved change starts a sync by
    // itself, spec 008), picker_view {list,tile} (default list: the layout of the attach file browser, spec 009),
    // list_layout {list,tile} (default list: the layout of the note list, spec 010).
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
    // A note in sync shows the server's title (title_for); the first line of the text is only the fallback for a
    // server note that has none. Local edits keep the title the user entered.
    // True if stored synced notes may carry a title that is not the server's (stored by an older
    // version, or a note with neither text nor title): the next sync then pulls the full list once
    // and calls mark_titles_refreshed() afterwards.
    bool titles_need_refresh() const;
    bool mark_titles_refreshed();
    PullApplyResult apply_remote(const std::vector<ApiNote>& listing);

    // ---- local edits (work offline; every change leaves the note pending until pushed) ------------
    // create_local / save_local take the title the user entered (spec 004): it is trimmed and validated
    // (validate_title) and stored as typed, never derived from the text. Checks run in the order title, content
    // (must contain more than whitespace), category; the first failure is returned and nothing is stored, so
    // the previous version stays. A new note with neither title nor text is `content_empty`.
    // save_local / set_favorite / set_category: mark the note `edited` with a fresh modified time and recompute
    // the preview; saving identical values changes nothing. set_favorite, set_category and toggle_task keep the
    // stored title. A failed write leaves the previous version untouched and reports StorageError.
    // mark_deleted: a note that never reached the server disappears at once; a synced note is
    // kept as a hidden tombstone (`deleted`) until the server delete succeeds.
    EditResult create_local(const std::string& title, const std::string& content, const std::string& category,
                            bool favorite, int64_t now);
    EditResult save_local(int64_t local_id, const std::string& title, const std::string& content,
                          const std::string& category, bool favorite, int64_t now);
    // Like save_local, for the editor (spec 008). A sync that ran while the editor was open may have replaced the
    // note with a newer server version; saving the editor's older text over it would then be pushed with the NEW
    // validator and silently overwrite the server's text (Principle VI). So when the stored text, category or
    // favorite mark differ from `base` (what the editor loaded) nothing is written and the result is
    // EditError::ChangedOnServer ("changed_on_server"). Only the text, category and favorite are compared: a new
    // ETag with the same text (the reader's own push, confirmed meanwhile) is an ordinary save.
    EditResult save_local_if_unchanged(int64_t local_id, const EditBase& base, const std::string& title,
                                       const std::string& content, const std::string& category, bool favorite,
                                       int64_t now);
    // Stores `content` (the editor's text for a note a sync changed meanwhile) as a NEW pending note titled
    // `title` + the conflict suffix, numbered " #n" while that title exists (the rule resolve_conflict uses, shared
    // code). Staged attachments the text names are bound to the copy; downloaded ones of `original_local_id` it
    // names are duplicated for it. Validation as create_local. The original is not touched.
    EditResult save_edit_as_conflict_copy(int64_t original_local_id, const std::string& title, const std::string& content,
                                          const std::string& category, bool favorite, int64_t now);
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
        std::string title, content, category;
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

    // ---- attachments: queries and the download side (spec 002, data-model.md) --------------------------------
    // A row per attachment the app knows about. Paths are validated (attachment_ref.h) and never become local
    // paths: the local file is always "<id>.<ext>". Rows follow their note (ON DELETE CASCADE).
    std::optional<Attachment> get_attachment(int64_t id) const;
    std::optional<Attachment> attachment_for(int64_t note_local_id, const std::string& remote_path) const;
    // The row for a reference found in a note's text, created as `remote` (nothing downloaded, size unknown)
    // if there is none; nullopt for an unknown note or an invalid path. display_name and kind are derived from
    // the path (control characters removed, at most 255 characters; image extensions jpg jpeg png gif bmp webp).
    std::optional<Attachment> ensure_attachment(int64_t note_local_id, const std::string& remote_path);
    // Remembers a size learned from a HEAD answer (creates a `remote` row if needed). Refused for a negative size,
    // an unknown note, and, for a row that is not `remote`, a size over kMaxAttachmentBytes.
    bool record_size(int64_t note_local_id, const std::string& remote_path, int64_t size);
    // The file `<id>.<ext>` (local_file) was stored completely: the row becomes `downloaded` with its size and
    // loses any error. Creates the row if needed. Refused when size is outside [0, kMaxAttachmentBytes], when
    // local_file is not "<row id>" or "<row id>.<[a-z0-9]{1,8}>", or for an unknown note or invalid path.
    bool record_downloaded(int64_t note_local_id, const std::string& remote_path, int64_t size, const std::string& local_file);
    // Sets the failure reason of a row ("" clears it; otherwise one of the closed set in types.h). By row id, or
    // (id 0) by note and path, creating a `remote` row if needed. False for an unknown row, note or key.
    bool set_attachment_error(int64_t id, int64_t note_local_id, const std::string& remote_path, const std::string& error_key);
    // File names of every row that names a local file (for the janitor).
    std::vector<std::string> referenced_local_files() const;
    // Deletes the `downloaded` rows of a note whose path its text no longer references (a pull changed the
    // text). `remote`, `pending` and `staged` rows are kept; the files are removed by the janitor.
    void purge_unreferenced_downloads(int64_t note_local_id);

    // ---- attachments: adding and uploading (spec 002 US5, data-model.md state machine) ---------------------
    // A file chosen in the editor: first a `staged` row without a note and without a file (the id names the
    // file), then set_staged_file once the copy is complete. A staged row is bound to the note by saving the
    // note whose text carries its token: create_local and save_local do that INSIDE the save transaction
    // (tokens of unknown rows, of rows already bound to another note and of rows without a file are ignored),
    // so a saved token never points at an unbound row. The row becomes `pending` (waiting for the upload).
    std::optional<Attachment> stage_attachment(const std::string& display_name, int64_t now);
    bool set_staged_file(int64_t id, int64_t size, const std::string& local_file);
    bool drop_staged(int64_t id);   // a staged row that has no note; false otherwise
    // Cleanup after the editor was left or the app (re)started: staged rows no note's text names are dropped;
    // one a saved note does name (a crash between the save and the binding) is bound after all. The files of
    // dropped rows are removed by the janitor.
    bool discard_staged();
    std::vector<Attachment> pending_attachments(int64_t note_local_id) const;   // state pending, oldest first
    // Removes from a note's text every token no pending attachment of that note answers to (a dangling reference
    // that could never be uploaded); the status and `modified` stay. Returns false on a store failure.
    bool drop_dead_tokens(int64_t note_local_id);
    // The server stored the file under `remote_path`: ONE transaction sets path, size and state `downloaded`
    // (the local file is kept), replaces the token in the note's text by the percent-encoded path and
    // recomputes title and excerpt. `modified`, `status` and the ETag stay as they are. Idempotent; refused
    // for a row that is not pending, an invalid path or a size outside [0, kMaxAttachmentBytes].
    bool record_uploaded(int64_t id, const std::string& remote_path, int64_t size);

    // ---- sync bookkeeping (account row) ------------------------------------------------------
    bool set_collection_state(const std::optional<std::string>& etag, int64_t last_modified,
                              const std::string& api_versions);
    bool record_sync_result(const SyncResult& result, int64_t now);

private:
    NoteStore() = default;
    EditResult update_locked(int64_t local_id, const std::optional<std::string>& title,
                             const std::optional<std::string>& content,
                             const std::optional<std::string>& category, const std::optional<bool>& favorite,
                             int64_t now);

    // Row mapping and the shared work of the attachment operations (note_store_attachments.cpp); the caller
    // holds mutex_.
    std::optional<Attachment> attachment_by_key_locked(int64_t note_local_id, const std::string& remote_path) const;
    std::optional<Attachment> ensure_attachment_locked(int64_t note_local_id, const std::string& remote_path);
    void purge_unreferenced_downloads_locked(int64_t note_local_id, const std::string& content);
    bool bind_staged_locked(int64_t note_local_id, const std::string& content);
    // Conflict copy: moves the pending attachments of `original` to `copy` and duplicates the downloaded ones its
    // text names for the copy; returns false on a store failure (the caller rolls back).
    // The first title "<base_title> (conflict DATE)", then " #2", " #3" ... that no note has (shared with
    // resolve_conflict). False when the database cannot be asked.
    bool unused_conflict_title_locked(const std::string& base_title, int64_t now, std::string* out);
    bool copy_attachments_for_conflict_locked(int64_t original, int64_t copy, const Note& local, int64_t now);

    sqlite3* db_ = nullptr;
    int fail_nth_write_ = 0;
    int write_counter_ = 0;
    static int fault_authorizer(void* user, int action, const char*, const char*, const char*, const char*);
    std::string path_;
    mutable std::mutex mutex_;
};

}  // namespace pbnotes
