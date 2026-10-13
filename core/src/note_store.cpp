#include "pbnotes/note_store.h"

#include "pbnotes/json.h"
#include "pbnotes/log.h"
#include "pbnotes/markdown.h"
#include "pbnotes/title.h"
#include "store_sql.h"
#include <sqlite3.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>

namespace pbnotes {

using sql::exec;
using sql::query_text;
using sql::sqlite_error;
using sql::Stmt;

namespace {

const char* kSchemaSql = R"SQL(
CREATE TABLE IF NOT EXISTS account(
    id INTEGER PRIMARY KEY CHECK(id = 1),
    server_url TEXT NOT NULL,
    username TEXT NOT NULL,
    password TEXT NOT NULL,
    collection_etag TEXT,
    collection_last_modified INTEGER NOT NULL DEFAULT 0,
    api_versions TEXT NOT NULL DEFAULT '',
    last_sync_at INTEGER,
    last_sync_outcome TEXT NOT NULL DEFAULT 'never'
        CHECK(last_sync_outcome IN ('never','success','partial','failed','offline','maintenance')),
    last_sync_message TEXT NOT NULL DEFAULT '',
    last_sync_counts TEXT NOT NULL DEFAULT '{}'
);
CREATE TABLE IF NOT EXISTS notes(
    local_id INTEGER PRIMARY KEY AUTOINCREMENT,
    remote_id INTEGER UNIQUE,
    status TEXT NOT NULL CHECK(status IN ('void','edited','deleted')),
    title TEXT NOT NULL,
    content TEXT NOT NULL,
    category TEXT NOT NULL DEFAULT '',
    favorite INTEGER NOT NULL DEFAULT 0 CHECK(favorite IN (0,1)),
    modified INTEGER NOT NULL,
    etag TEXT,
    excerpt TEXT NOT NULL DEFAULT '',
    CHECK(remote_id IS NULL OR etag IS NOT NULL)
);
CREATE INDEX IF NOT EXISTS idx_notes_status ON notes(status);
CREATE INDEX IF NOT EXISTS idx_notes_category ON notes(category);
CREATE INDEX IF NOT EXISTS idx_notes_favorite ON notes(favorite);
CREATE INDEX IF NOT EXISTS idx_notes_modified ON notes(modified);
CREATE INDEX IF NOT EXISTS idx_notes_remote_id ON notes(remote_id);
CREATE TABLE IF NOT EXISTS settings(
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS attachments(
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    note_local_id INTEGER REFERENCES notes(local_id) ON DELETE CASCADE,
    remote_path TEXT,
    display_name TEXT NOT NULL,
    kind TEXT NOT NULL CHECK(kind IN ('image','file')),
    size INTEGER,
    state TEXT NOT NULL CHECK(state IN ('staged','pending','remote','downloaded')),
    error_key TEXT NOT NULL DEFAULT '',
    local_file TEXT NOT NULL DEFAULT '',
    created INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS attachments_note ON attachments(note_local_id);
CREATE UNIQUE INDEX IF NOT EXISTS attachments_path ON attachments(note_local_id, remote_path)
    WHERE remote_path IS NOT NULL;
)SQL";

// Schema 2 -> 3 (spec 004, data-model.md): data only. Unsynced notes carried the raw first line as their title;
// they keep what the list showed (legacy_title: no heading marker, within the title rules) so the first push
// never sends a guess, and every preview is recomputed with the rule that keeps a first line which is not the
// title. Nothing else of any row changes. Runs inside the migration transaction; returns false on failure.
bool migrate_titles_and_previews(sqlite3* db, std::string* error) {
    struct Row { int64_t id; std::string status, title, content, excerpt; };
    std::vector<Row> rows;
    {
        Stmt st(db, "SELECT local_id,status,title,content,excerpt FROM notes");
        if (!st.ok()) { *error = sqlite_error(db); return false; }
        while (st.step() == SQLITE_ROW) rows.push_back({st.int64(0), st.text(1), st.text(2), st.text(3), st.text(4)});
    }
    Stmt up(db, "UPDATE notes SET title=?1, excerpt=?2 WHERE local_id=?3");
    if (!up.ok()) { *error = sqlite_error(db); return false; }
    for (const Row& r : rows) {
        const std::string title = r.status == "edited" ? legacy_title(r.title) : r.title;
        const std::string excerpt = make_excerpt(r.content, title);
        if (title == r.title && excerpt == r.excerpt) continue;
        sqlite3_reset(up.get());
        up.bind_text(1, title);
        up.bind_text(2, excerpt);
        up.bind_int64(3, r.id);
        if (up.step() != SQLITE_DONE) { *error = sqlite_error(db); return false; }
    }
    return true;
}

bool file_size(const std::string& path, long long* size) {
    struct stat st;
    if (::stat(path.c_str(), &st) != 0) return false;
    *size = st.st_size;
    return true;
}

bool copy_file(const std::string& from, const std::string& to) {
    std::ifstream in(from, std::ios::binary);
    std::ofstream out(to, std::ios::binary | std::ios::trunc);
    if (!in || !out) return false;
    out << in.rdbuf();
    out.flush();
    return static_cast<bool>(out);
}

// The title shown for a note that is in sync with the server: the server's title (its file name, the
// one the web and Android apps show); only a note the server gave no title is shown by its first line.
// Local edits keep the title the user entered (spec 004) until the server confirms them.
std::string title_for(const ApiNote& n) {
    std::string t = derive_title(n.server_title);
    if (t.empty()) t = strip_heading_marker(derive_title(n.content));   // as the server derives a title: no heading marker
    return t;
}

const std::map<std::string, std::string>& setting_defaults() {
    static const std::map<std::string, std::string> d = {
        {"font_size", "medium"}, {"sort_order", "modified"}, {"view", "all"}, {"rotation", "auto"}, {"sync_after_save", "on"},
        {"picker_view", "list"}, {"list_layout", "list"}};
    return d;
}

bool valid_setting(const std::string& key, const std::string& value) {
    if (key == "font_size") return value == "small" || value == "medium" || value == "large";
    if (key == "sort_order") return value == "modified" || value == "title";
    if (key == "rotation") return value == "auto" || value == "portrait" || value == "landscape";
    if (key == "sync_after_save") return value == "on" || value == "off";
    if (key == "picker_view") return value == "list" || value == "tile";
    if (key == "list_layout") return value == "list" || value == "tile";
    if (key == "view") {
        if (value == "all" || value == "favorites" || value == "uncategorized") return true;
        return value.size() > 9 && value.compare(0, 9, "category:") == 0;
    }
    return false;
}

}  // namespace

std::unique_ptr<NoteStore> NoteStore::open(const std::string& path, std::string* error, const OpenOptions& options) {
    auto fail = [&](const std::string& msg) -> std::unique_ptr<NoteStore> {
        if (error) *error = msg;
        return nullptr;
    };

    long long size = 0;
    const bool existed_nonempty = file_size(path, &size) && size > 0;

    std::unique_ptr<NoteStore> store(new NoteStore());
    store->path_ = path;
    if (sqlite3_open_v2(path.c_str(), &store->db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        return fail("cannot open database: " + sqlite_error(store->db_));
    }
    sqlite3* db = store->db_;
    sqlite3_busy_timeout(db, 5000);

    std::string err, value;
    // The first real statement fails with "file is not a database" for garbage files.
    if (!query_text(db, "PRAGMA user_version", &value, &err)) return fail("database unusable: " + err);
    const int version = std::atoi(value.c_str());

    // FAT storage: WAL's shared memory is unreliable there; TRUNCATE journals are safe.
    if (!query_text(db, "PRAGMA journal_mode=TRUNCATE", &value, &err)) return fail("journal mode: " + err);
    if (!exec(db, "PRAGMA synchronous=FULL; PRAGMA foreign_keys=ON; PRAGMA secure_delete=ON;", &err))
        return fail("pragmas: " + err);

    if (version > kSchemaVersion) return fail("database was written by a newer version");

    if (version < kSchemaVersion) {
        const bool migrating_existing = existed_nonempty;
        const std::string backup = path + ".bak";
        if (migrating_existing) {
            if (!copy_file(path, backup)) return fail("cannot write migration backup");
            if (options.after_backup_hook && !options.after_backup_hook(backup))
                return fail("migration failed; backup kept at " + backup);
        }
        if (!exec(db, "BEGIN IMMEDIATE", &err)) return fail("migration: " + err);
        if (!exec(db, kSchemaSql, &err) || (version >= 1 && !migrate_titles_and_previews(db, &err)) ||
            !exec(db, ("PRAGMA user_version=" + std::to_string(kSchemaVersion)).c_str(), &err)) {
            std::string ignored;
            exec(db, "ROLLBACK", &ignored);
            return fail("migration: " + err);
        }
        if (!exec(db, "COMMIT", &err)) return fail("migration: " + err);
        if (!query_text(db, "PRAGMA integrity_check", &value, &err) || value != "ok")
            return fail("integrity check failed: " + (value.empty() ? err : value));
        if (migrating_existing) std::remove(backup.c_str());  // no lingering plaintext copy
    }
    sqlite3_busy_timeout(db, 5000);
    return store;
}

NoteStore::~NoteStore() {
    if (db_) sqlite3_close(db_);
}

int NoteStore::schema_version() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string v;
    query_text(db_, "PRAGMA user_version", &v, nullptr);
    return std::atoi(v.c_str());
}

std::string NoteStore::pragma(const std::string& name) const {
    for (char c : name)
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return std::string();
    std::lock_guard<std::mutex> lock(mutex_);
    std::string v;
    query_text(db_, "PRAGMA " + name, &v, nullptr);
    return v;
}

std::string NoteStore::get_setting(const std::string& key) const {
    const auto& defaults = setting_defaults();
    auto def = defaults.find(key);
    if (def == defaults.end()) return std::string();
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    std::string result = def->second;
    if (sqlite3_prepare_v2(db_, "SELECT value FROM settings WHERE key=?1", -1, &st, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char* t = sqlite3_column_text(st, 0);
            if (t) result = reinterpret_cast<const char*>(t);
        }
    }
    sqlite3_finalize(st);
    return result;
}

bool NoteStore::set_setting(const std::string& key, const std::string& value) {
    if (!valid_setting(key, value)) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db_, "INSERT INTO settings(key,value) VALUES(?1,?2) "
                                "ON CONFLICT(key) DO UPDATE SET value=excluded.value",
                           -1, &st, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(st, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(st, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    const bool ok = sqlite3_step(st) == SQLITE_DONE;
    sqlite3_finalize(st);
    return ok;
}

// ---- account -------------------------------------------------------------------------------

namespace {

std::string counts_to_json(const SyncCounts& c) {
    return "{\"pushed\":" + std::to_string(c.pushed) + ",\"pulled\":" + std::to_string(c.pulled) +
           ",\"deleted\":" + std::to_string(c.deleted) + ",\"conflicts\":" + std::to_string(c.conflicts) +
           ",\"recreated\":" + std::to_string(c.recreated) + ",\"restored\":" + std::to_string(c.restored) +
           ",\"attachments_uploaded\":" + std::to_string(c.attachments_uploaded) +
           ",\"attachments_failed\":" + std::to_string(c.attachments_failed) + "}";
}

SyncCounts counts_from_json(const std::string& text) {
    SyncCounts c;
    auto v = json::parse(text);
    if (!v) return c;
    c.pushed = static_cast<int>(v->get_int("pushed").value_or(0));
    c.pulled = static_cast<int>(v->get_int("pulled").value_or(0));
    c.deleted = static_cast<int>(v->get_int("deleted").value_or(0));
    c.conflicts = static_cast<int>(v->get_int("conflicts").value_or(0));
    c.recreated = static_cast<int>(v->get_int("recreated").value_or(0));
    c.restored = static_cast<int>(v->get_int("restored").value_or(0));
    c.attachments_uploaded = static_cast<int>(v->get_int("attachments_uploaded").value_or(0));
    c.attachments_failed = static_cast<int>(v->get_int("attachments_failed").value_or(0));
    return c;
}

}  // namespace

bool NoteStore::save_account(const Account& a) {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_,
            "INSERT INTO account(id,server_url,username,password,collection_etag,collection_last_modified,"
            "api_versions,last_sync_at,last_sync_outcome,last_sync_message,last_sync_counts) "
            "VALUES(1,?1,?2,?3,?4,?5,?6,?7,?8,?9,?10) "
            "ON CONFLICT(id) DO UPDATE SET server_url=excluded.server_url, username=excluded.username, "
            "password=excluded.password, collection_etag=excluded.collection_etag, "
            "collection_last_modified=excluded.collection_last_modified, api_versions=excluded.api_versions, "
            "last_sync_at=excluded.last_sync_at, last_sync_outcome=excluded.last_sync_outcome, "
            "last_sync_message=excluded.last_sync_message, last_sync_counts=excluded.last_sync_counts");
    if (!st.ok()) return false;
    st.bind_text(1, a.server_url);
    st.bind_text(2, a.username);
    st.bind_text(3, a.password);
    if (a.collection_etag) st.bind_text(4, *a.collection_etag); else st.bind_null(4);
    st.bind_int64(5, a.collection_last_modified);
    st.bind_text(6, a.api_versions);
    if (a.last_sync_at) st.bind_int64(7, *a.last_sync_at); else st.bind_null(7);
    st.bind_text(8, to_string(a.last_sync_outcome == SyncOutcome::Running ? SyncOutcome::Never : a.last_sync_outcome));
    st.bind_text(9, a.last_sync_message);
    st.bind_text(10, counts_to_json(a.last_sync_counts));
    const bool ok = st.step() == SQLITE_DONE;
    if (ok) log::event("account.saved", {});
    return ok;
}

std::optional<Account> NoteStore::get_account() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_,
            "SELECT server_url,username,password,collection_etag,collection_last_modified,api_versions,"
            "last_sync_at,last_sync_outcome,last_sync_message,last_sync_counts FROM account WHERE id=1");
    if (!st.ok() || st.step() != SQLITE_ROW) return std::nullopt;
    Account a;
    a.server_url = st.text(0);
    a.username = st.text(1);
    a.password = st.text(2);
    if (!st.is_null(3)) a.collection_etag = st.text(3);
    a.collection_last_modified = st.int64(4);
    a.api_versions = st.text(5);
    if (!st.is_null(6)) a.last_sync_at = st.int64(6);
    a.last_sync_outcome = sync_outcome_from_string(st.text(7)).value_or(SyncOutcome::Never);
    a.last_sync_message = st.text(8);
    a.last_sync_counts = counts_from_json(st.text(9));
    return a;
}

namespace {
const char* const kDraftKeys = "('setup_draft_url','setup_draft_user','setup_draft_password')";
const char* const kClearDraftSql1 =
    "UPDATE settings SET value='' WHERE key IN ('setup_draft_url','setup_draft_user','setup_draft_password')";
const char* const kClearDraftSql2 =
    "DELETE FROM settings WHERE key IN ('setup_draft_url','setup_draft_user','setup_draft_password')";
}  // namespace

bool NoteStore::save_setup_draft(const SetupDraft& draft) {
    if (draft.url.empty() && draft.username.empty() && draft.password.empty()) return clear_setup_draft();
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return false;
    bool ok = true;
    const std::pair<const char*, const std::string*> rows[] = {{"setup_draft_url", &draft.url},
                                                                {"setup_draft_user", &draft.username},
                                                                {"setup_draft_password", &draft.password}};
    for (const auto& row : rows) {
        Stmt st(db_, "INSERT INTO settings(key,value) VALUES(?1,?2) "
                     "ON CONFLICT(key) DO UPDATE SET value=excluded.value");
        ok = ok && st.ok();
        if (!ok) break;
        st.bind_text(1, row.first);
        st.bind_text(2, *row.second);
        ok = st.step() == SQLITE_DONE;
    }
    if (!ok || !exec(db_, "COMMIT", &err)) {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return false;
    }
    return true;
}

std::optional<NoteStore::SetupDraft> NoteStore::get_setup_draft() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_, (std::string("SELECT key, value FROM settings WHERE key IN ") + kDraftKeys).c_str());
    if (!st.ok()) return std::nullopt;
    SetupDraft d;
    bool any = false;
    while (st.step() == SQLITE_ROW) {
        const std::string key = st.text(0), value = st.text(1);
        if (key == "setup_draft_url") d.url = value;
        else if (key == "setup_draft_user") d.username = value;
        else if (key == "setup_draft_password") d.password = value;
        any = any || !value.empty();
    }
    if (!any) return std::nullopt;
    return d;
}

bool NoteStore::clear_setup_draft() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return false;
    if (!exec(db_, kClearDraftSql1, &err) || !exec(db_, kClearDraftSql2, &err)) {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return false;
    }
    return exec(db_, "COMMIT", &err);
}

bool NoteStore::forget_account() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return false;
    // Overwrite first, then delete: with secure_delete=ON freed space is zeroed as well.
    const bool ok = exec(db_, "UPDATE account SET password='', username='', server_url='' WHERE id=1", &err) &&
                    exec(db_, "DELETE FROM account WHERE id=1", &err) &&
                    exec(db_, kClearDraftSql1, &err) && exec(db_, kClearDraftSql2, &err);
    if (!ok) {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return false;
    }
    if (!exec(db_, "COMMIT", &err)) return false;
    log::event("account.forgotten", {});
    return true;
}

int NoteStore::account_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string v;
    query_text(db_, "SELECT COUNT(*) FROM account", &v, nullptr);
    return std::atoi(v.c_str());
}

// ---- notes -----------------------------------------------------------------------------------

namespace {

const char* kNoteColumns = "local_id,remote_id,status,title,content,category,favorite,modified,etag,excerpt";

Note read_note(const Stmt& st) {
    Note n;
    n.local_id = st.int64(0);
    if (!st.is_null(1)) n.remote_id = st.int64(1);
    n.status = note_status_from_string(st.text(2)).value_or(NoteStatus::Void);
    n.title = st.text(3);
    n.content = st.text(4);
    n.category = st.text(5);
    n.favorite = st.int64(6) != 0;
    n.modified = st.int64(7);
    if (!st.is_null(8)) n.etag = st.text(8);
    n.excerpt = st.text(9);
    return n;
}

}  // namespace

std::optional<Note> NoteStore::get(int64_t local_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_, (std::string("SELECT ") + kNoteColumns + " FROM notes WHERE local_id=?1").c_str());
    if (!st.ok()) return std::nullopt;
    st.bind_int64(1, local_id);
    if (st.step() != SQLITE_ROW) return std::nullopt;
    return read_note(st);
}

std::optional<Note> NoteStore::get_by_remote_id(int64_t remote_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_, (std::string("SELECT ") + kNoteColumns + " FROM notes WHERE remote_id=?1").c_str());
    if (!st.ok()) return std::nullopt;
    st.bind_int64(1, remote_id);
    if (st.step() != SQLITE_ROW) return std::nullopt;
    return read_note(st);
}

std::vector<NoteSummary> NoteStore::list(const ListQuery& query) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<NoteSummary> out;

    std::string where = "status != 'deleted'";
    std::string category_arg;
    bool by_category = false;
    if (query.view == "favorites") where += " AND favorite=1";
    else if (query.view == "uncategorized") where += " AND category=''";
    else if (query.view.compare(0, 9, "category:") == 0 && query.view.size() > 9) {
        where += " AND category=?1";
        category_arg = query.view.substr(9);
        by_category = true;
    }

    // Substring search is done here (not with SQL LIKE) so that non-ASCII letters fold correctly
    // and %, _ and quotes are plain characters.
    std::string needle = fold_case(query.search);
    {
        size_t b = needle.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) needle.clear();
    }
    const bool searching = !needle.empty();

    const std::string sql = std::string("SELECT local_id,title,category,excerpt,favorite,modified,status") +
                            (searching ? ",content" : "") + " FROM notes WHERE " + where;
    Stmt st(db_, sql.c_str());
    if (!st.ok()) return out;
    if (by_category) st.bind_text(1, category_arg);
    while (st.step() == SQLITE_ROW) {
        if (searching) {
            const std::string hay = fold_case(st.text(1)) + "\n" + fold_case(st.text(7));
            if (hay.find(needle) == std::string::npos) continue;
        }
        NoteSummary s;
        s.local_id = st.int64(0);
        s.title = st.text(1);
        s.category = st.text(2);
        s.excerpt = st.text(3);
        s.favorite = st.int64(4) != 0;
        s.modified = st.int64(5);
        s.pending = st.text(6) != "void";
        out.push_back(std::move(s));
    }
    if (query.sort == "title") {
        std::sort(out.begin(), out.end(), [](const NoteSummary& a, const NoteSummary& b) {
            const std::string fa = fold_case(a.title), fb = fold_case(b.title);
            return fa != fb ? fa < fb : a.local_id < b.local_id;
        });
    } else {
        std::sort(out.begin(), out.end(), [](const NoteSummary& a, const NoteSummary& b) {
            return a.modified != b.modified ? a.modified > b.modified : a.local_id > b.local_id;
        });
    }
    return out;
}

std::vector<CategoryCount> NoteStore::categories() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CategoryCount> out;
    Stmt st(db_, "SELECT category, COUNT(*) FROM notes WHERE status != 'deleted' AND category != '' GROUP BY category");
    while (st.ok() && st.step() == SQLITE_ROW) out.push_back({st.text(0), static_cast<int>(st.int64(1))});
    std::sort(out.begin(), out.end(), [](const CategoryCount& a, const CategoryCount& b) {
        const std::string fa = fold_case(a.name), fb = fold_case(b.name);
        return fa != fb ? fa < fb : a.name < b.name;
    });
    return out;
}

bool NoteStore::titles_need_refresh() const {
    std::lock_guard<std::mutex> lock(mutex_);
    {
        Stmt st(db_, "SELECT 1 FROM notes WHERE remote_id IS NOT NULL AND status='void' AND content='' AND title='' LIMIT 1");
        if (st.ok() && st.step() == SQLITE_ROW) return true;
    }
    {
        Stmt marker(db_, "SELECT 1 FROM settings WHERE key='titles_from_server'");
        if (marker.ok() && marker.step() == SQLITE_ROW) return false;
    }
    Stmt any(db_, "SELECT 1 FROM notes WHERE remote_id IS NOT NULL AND status='void' LIMIT 1");
    return any.ok() && any.step() == SQLITE_ROW;
}

bool NoteStore::mark_titles_refreshed() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    return exec(db_, "INSERT OR REPLACE INTO settings(key,value) VALUES('titles_from_server','1')", &err);
}

PullApplyResult NoteStore::apply_remote(const std::vector<ApiNote>& listing) {
    PullApplyResult result;
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return result;

    struct Local {
        int64_t local_id;
        NoteStatus status;
        std::string content, category, etag, title;
        bool favorite;
        int64_t modified;
    };
    std::map<int64_t, Local> locals;
    {
        Stmt st(db_, "SELECT remote_id,local_id,status,content,category,etag,favorite,modified,title FROM notes WHERE remote_id IS NOT NULL");
        while (st.ok() && st.step() == SQLITE_ROW)
            locals[st.int64(0)] = {st.int64(1), note_status_from_string(st.text(2)).value_or(NoteStatus::Void), st.text(3),
                                   st.text(4), st.text(5), st.text(8), st.int64(6) != 0, st.int64(7)};
    }

    bool ok = true;
    std::map<int64_t, bool> in_listing;
    for (const ApiNote& n : listing) {
        in_listing[n.id] = true;
        auto it = locals.find(n.id);
        if (it == locals.end()) {
            if (n.pruned) {
                result.need_full_fetch.push_back(n.id);
                continue;
            }
            Stmt ins(db_,
                     "INSERT INTO notes(remote_id,status,title,content,category,favorite,modified,etag,excerpt) "
                     "VALUES(?1,'void',?2,?3,?4,?5,?6,?7,?8)");
            ok = ok && ins.ok();
            if (!ok) break;
            ins.bind_int64(1, n.id);
            ins.bind_text(2, title_for(n));
            ins.bind_text(3, n.content);
            ins.bind_text(4, n.category);
            ins.bind_int64(5, n.favorite ? 1 : 0);
            ins.bind_int64(6, n.modified);
            ins.bind_text(7, n.etag);
            ins.bind_text(8, make_excerpt(n.content, title_for(n)));
            ok = ins.step() == SQLITE_DONE;
            if (ok) ++result.inserted;
            continue;
        }
        const Local& l = it->second;
        if (n.pruned || l.status != NoteStatus::Void) continue;  // unchanged, or local changes win
        if (n.content == l.content && n.category == l.category && n.favorite == l.favorite &&
            n.modified == l.modified && n.etag == l.etag && title_for(n) == l.title)
            continue;
        Stmt up(db_,
                "UPDATE notes SET title=?1,content=?2,category=?3,favorite=?4,modified=?5,etag=?6,excerpt=?7 "
                "WHERE local_id=?8");
        ok = up.ok();
        if (!ok) break;
        up.bind_text(1, title_for(n));
        up.bind_text(2, n.content);
        up.bind_text(3, n.category);
        up.bind_int64(4, n.favorite ? 1 : 0);
        up.bind_int64(5, n.modified);
        up.bind_text(6, n.etag);
        up.bind_text(7, make_excerpt(n.content, title_for(n)));
        up.bind_int64(8, l.local_id);
        ok = up.step() == SQLITE_DONE;
        if (ok) {
            ++result.updated;
            if (n.content != l.content) purge_unreferenced_downloads_locked(l.local_id, n.content);
        }
    }

    if (ok) {
        for (const auto& kv : locals) {
            if (in_listing.count(kv.first) || kv.second.status != NoteStatus::Void) continue;
            Stmt del(db_, "DELETE FROM notes WHERE local_id=?1");
            ok = del.ok();
            if (!ok) break;
            del.bind_int64(1, kv.second.local_id);
            ok = del.step() == SQLITE_DONE;
            if (ok) ++result.removed;
        }
    }

    if (!ok || !exec(db_, "COMMIT", &err)) {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return PullApplyResult();
    }
    return result;
}

// ---- local edits ---------------------------------------------------------------------------------

namespace {

// Number of code points of strictly valid UTF-8 (no overlong forms, surrogates or values above U+10FFFF), or -1.
long strict_utf8_length(const std::string& s) {
    long chars = 0;
    for (size_t i = 0; i < s.size(); ++chars) {
        const unsigned char b = static_cast<unsigned char>(s[i]);
        size_t n;
        uint32_t cp;
        if (b < 0x80) { n = 1; cp = b; }
        else if (b >= 0xC2 && b <= 0xDF) { n = 2; cp = b & 0x1F; }
        else if ((b >> 4) == 0xE) { n = 3; cp = b & 0x0F; }
        else if (b >= 0xF0 && b <= 0xF4) { n = 4; cp = b & 0x07; }
        else return -1;
        if (i + n > s.size()) return -1;
        for (size_t k = 1; k < n; ++k) {
            const unsigned char c = static_cast<unsigned char>(s[i + k]);
            if ((c & 0xC0) != 0x80) return -1;
            cp = (cp << 6) | (c & 0x3F);
        }
        if ((n == 3 && cp < 0x800) || (n == 4 && cp < 0x10000) || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            return -1;
        i += n;
    }
    return chars;
}

bool is_title_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v'; }

}  // namespace

std::string trim_title(const std::string& title) {
    size_t b = 0, e = title.size();
    while (b < e && is_title_space(title[b])) ++b;
    while (e > b && is_title_space(title[e - 1])) --e;
    return title.substr(b, e - b);
}

std::string validate_title(const std::string& title) {
    const std::string t = trim_title(title);
    if (t.empty()) return "title_empty";
    const long chars = strict_utf8_length(t);
    if (chars < 0) return "title_invalid";
    for (char c : t) {
        const unsigned char b = static_cast<unsigned char>(c);
        if (b < 0x20 || b == 0x7F) return "title_control_chars";
    }
    if (chars > 255) return "title_too_long";
    return std::string();
}

std::string validate_category(const std::string& c) {
    size_t chars = 0;
    bool control = false;
    for (size_t i = 0; i < c.size();) {
        const unsigned char b = static_cast<unsigned char>(c[i]);
        size_t n;
        if (b < 0x80) n = 1;
        else if ((b >> 5) == 0x6 && b >= 0xC2) n = 2;
        else if ((b >> 4) == 0xE) n = 3;
        else if ((b >> 3) == 0x1E && b <= 0xF4) n = 4;
        else return "category_invalid";
        if (i + n > c.size()) return "category_invalid";
        for (size_t k = 1; k < n; ++k)
            if ((static_cast<unsigned char>(c[i + k]) & 0xC0) != 0x80) return "category_invalid";
        if (b < 0x20 || b == 0x7F) control = true;
        ++chars;
        i += n;
    }
    if (control) return "category_control_chars";
    if (!c.empty() && (c.front() == ' ' || c.back() == ' ')) return "category_spaces";
    if (chars > 255) return "category_too_long";
    return std::string();
}

namespace {

bool only_whitespace(const std::string& s) {
    for (char c : s)
        if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v')) return false;
    return true;
}

EditResult edit_error(EditError e, const char* key) {
    EditResult r;
    r.error = e;
    r.error_key = key;
    return r;
}

}  // namespace

EditResult NoteStore::create_local(const std::string& title, const std::string& content, const std::string& category,
                                   bool favorite, int64_t now) {
    const std::string trimmed_title = trim_title(title);
    // neither title nor text: there is nothing to say about the title, it is simply an empty note
    if (only_whitespace(content) && trimmed_title.empty()) return edit_error(EditError::EmptyContent, "content_empty");
    const std::string title_error = validate_title(title);
    if (!title_error.empty()) {
        EditResult r;
        r.error = EditError::InvalidTitle;
        r.error_key = title_error;
        return r;
    }
    if (only_whitespace(content)) return edit_error(EditError::EmptyContent, "content_empty");
    const std::string cat_error = validate_category(category);
    if (!cat_error.empty()) {
        EditResult r;
        r.error = EditError::InvalidCategory;
        r.error_key = cat_error;
        return r;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return edit_error(EditError::StorageError, "storage_error");
    Stmt st(db_,
            "INSERT INTO notes(status,title,content,category,favorite,modified,excerpt) "
            "VALUES('edited',?1,?2,?3,?4,?5,?6)");
    bool ok = st.ok();
    if (ok) {
        st.bind_text(1, trimmed_title);
        st.bind_text(2, content);
        st.bind_text(3, category);
        st.bind_int64(4, favorite ? 1 : 0);
        st.bind_int64(5, now);
        st.bind_text(6, make_excerpt(content, trimmed_title));
        ok = st.step() == SQLITE_DONE;
    }
    const int64_t id = ok ? sqlite3_last_insert_rowid(db_) : 0;
    ok = ok && bind_staged_locked(id, content);   // in the same transaction: no saved token without its row
    if (!ok || !exec(db_, "COMMIT", &err)) {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return edit_error(EditError::StorageError, "storage_error");
    }
    EditResult r;
    r.local_id = id;
    return r;
}

EditResult NoteStore::update_locked(int64_t local_id, const std::optional<std::string>& title,
                                    const std::optional<std::string>& content,
                                    const std::optional<std::string>& category, const std::optional<bool>& favorite,
                                    int64_t now) {
    Note current;
    {
        Stmt st(db_, (std::string("SELECT ") + kNoteColumns + " FROM notes WHERE local_id=?1").c_str());
        if (!st.ok()) return edit_error(EditError::StorageError, "storage_error");
        st.bind_int64(1, local_id);
        if (st.step() != SQLITE_ROW) return edit_error(EditError::NotFound, "not_found");
        current = read_note(st);
    }
    if (current.status == NoteStatus::Deleted) return edit_error(EditError::NotFound, "not_found");

    std::string new_title = current.title;   // only an explicit title is judged; a stored legacy one is kept as is
    if (title) {
        const std::string title_error = validate_title(*title);
        if (!title_error.empty()) {
            EditResult r;
            r.error = EditError::InvalidTitle;
            r.error_key = title_error;
            r.local_id = local_id;
            return r;
        }
        new_title = trim_title(*title);
    }
    const std::string new_content = content.value_or(current.content);
    const std::string new_category = category.value_or(current.category);
    const bool new_favorite = favorite.value_or(current.favorite);
    if (only_whitespace(new_content)) return edit_error(EditError::EmptyContent, "content_empty");
    const std::string cat_error = validate_category(new_category);
    if (!cat_error.empty()) {
        EditResult r;
        r.error = EditError::InvalidCategory;
        r.error_key = cat_error;
        r.local_id = local_id;
        return r;
    }
    EditResult ok_result;
    ok_result.local_id = local_id;
    if (new_title == current.title && new_content == current.content && new_category == current.category &&
        new_favorite == current.favorite)
        return ok_result;  // nothing changed: nothing to push

    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return edit_error(EditError::StorageError, "storage_error");
    Stmt up(db_,
            "UPDATE notes SET status='edited',title=?1,content=?2,category=?3,favorite=?4,modified=?5,excerpt=?6 "
            "WHERE local_id=?7");
    bool ok = up.ok();
    if (ok) {
        up.bind_text(1, new_title);
        up.bind_text(2, new_content);
        up.bind_text(3, new_category);
        up.bind_int64(4, new_favorite ? 1 : 0);
        up.bind_int64(5, now);
        up.bind_text(6, make_excerpt(new_content, new_title));
        up.bind_int64(7, local_id);
        ok = up.step() == SQLITE_DONE;
    }
    ok = ok && bind_staged_locked(local_id, new_content);   // in the same transaction as the save
    if (!ok || !exec(db_, "COMMIT", &err)) {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        EditResult r = edit_error(EditError::StorageError, "storage_error");
        r.local_id = local_id;
        return r;
    }
    return ok_result;
}

EditResult NoteStore::save_local(int64_t local_id, const std::string& title, const std::string& content,
                                 const std::string& category, bool favorite, int64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    return update_locked(local_id, title, content, category, favorite, now);
}

EditResult NoteStore::save_local_if_unchanged(int64_t local_id, const EditBase& base, const std::string& title,
                                              const std::string& content, const std::string& category, bool favorite,
                                              int64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    {
        Stmt st(db_, "SELECT status,content,category,favorite FROM notes WHERE local_id=?1");
        if (!st.ok()) return edit_error(EditError::StorageError, "storage_error");
        st.bind_int64(1, local_id);
        if (st.step() != SQLITE_ROW) return edit_error(EditError::NotFound, "not_found");
        if (st.text(0) != "deleted") {   // a deleted note is refused by update_locked as before
            const bool same = st.text(1) == base.content && st.text(2) == base.category && (st.int64(3) != 0) == base.favorite;
            if (!same) {
                EditResult r = edit_error(EditError::ChangedOnServer, "changed_on_server");
                r.local_id = local_id;
                return r;
            }
        }
    }
    return update_locked(local_id, title, content, category, favorite, now);
}

EditResult NoteStore::save_edit_as_conflict_copy(int64_t original_local_id, const std::string& title,
                                                 const std::string& content, const std::string& category, bool favorite,
                                                 int64_t now) {
    const std::string trimmed_title = trim_title(title);
    const std::string title_error = validate_title(title);
    if (!title_error.empty()) {
        EditResult r;
        r.error = EditError::InvalidTitle;
        r.error_key = title_error;
        return r;
    }
    if (only_whitespace(content)) return edit_error(EditError::EmptyContent, "content_empty");
    const std::string cat_error = validate_category(category);
    if (!cat_error.empty()) {
        EditResult r;
        r.error = EditError::InvalidCategory;
        r.error_key = cat_error;
        return r;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return edit_error(EditError::StorageError, "storage_error");
    auto fail = [&]() {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return edit_error(EditError::StorageError, "storage_error");
    };
    std::string copy_title;
    if (!unused_conflict_title_locked(trimmed_title, now, &copy_title)) return fail();
    int64_t copy_id = 0;
    {
        Stmt ins(db_,
                 "INSERT INTO notes(status,title,content,category,favorite,modified,excerpt) "
                 "VALUES('edited',?1,?2,?3,?4,?5,?6)");
        if (!ins.ok()) return fail();
        ins.bind_text(1, copy_title);
        ins.bind_text(2, content);
        ins.bind_text(3, category);
        ins.bind_int64(4, favorite ? 1 : 0);
        ins.bind_int64(5, now);
        ins.bind_text(6, make_excerpt(content, trimmed_title));
        if (ins.step() != SQLITE_DONE) return fail();
        copy_id = sqlite3_last_insert_rowid(db_);
    }
    if (!bind_staged_locked(copy_id, content)) return fail();   // staged attachments the text names
    Note shown;   // what the copy's text names, for the duplicate rows
    shown.content = content;
    shown.title = trimmed_title;
    if (!copy_attachments_for_conflict_locked(original_local_id, copy_id, shown, now)) return fail();
    if (!exec(db_, "COMMIT", &err)) return fail();
    EditResult r;
    r.local_id = copy_id;
    return r;
}

EditResult NoteStore::set_favorite(int64_t local_id, bool favorite, int64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    return update_locked(local_id, std::nullopt, std::nullopt, std::nullopt, favorite, now);
}

EditResult NoteStore::set_category(int64_t local_id, const std::string& category, int64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    return update_locked(local_id, std::nullopt, std::nullopt, category, std::nullopt, now);
}

EditResult NoteStore::mark_deleted(int64_t local_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt find(db_, "SELECT status, remote_id FROM notes WHERE local_id=?1");
    if (!find.ok()) return edit_error(EditError::StorageError, "storage_error");
    find.bind_int64(1, local_id);
    if (find.step() != SQLITE_ROW) return edit_error(EditError::NotFound, "not_found");
    const std::string status = find.text(0);
    const bool never_synced = find.is_null(1);
    if (status == "deleted") {
        EditResult r;
        r.local_id = local_id;
        return r;
    }
    const char* sql = never_synced ? "DELETE FROM notes WHERE local_id=?1" : "UPDATE notes SET status='deleted' WHERE local_id=?1";
    Stmt st(db_, sql);
    if (!st.ok()) return edit_error(EditError::StorageError, "storage_error");
    st.bind_int64(1, local_id);
    if (st.step() != SQLITE_DONE) return edit_error(EditError::StorageError, "storage_error");
    EditResult r;
    r.local_id = local_id;
    return r;
}

EditResult NoteStore::toggle_task(int64_t local_id, int line_index, int64_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string content;
    {
        Stmt st(db_, "SELECT content, status FROM notes WHERE local_id=?1");
        if (!st.ok()) return edit_error(EditError::StorageError, "storage_error");
        st.bind_int64(1, local_id);
        if (st.step() != SQLITE_ROW || st.text(1) == "deleted") return edit_error(EditError::NotFound, "not_found");
        content = st.text(0);
    }
    const std::string toggled = toggle_task_item(content, line_index);
    if (toggled == content) {
        EditResult r;
        r.local_id = local_id;
        return r;
    }
    return update_locked(local_id, std::nullopt, toggled, std::nullopt, std::nullopt, now);
}

bool NoteStore::reset_local_data() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return false;
    bool ok = true;
    {
        Stmt st(db_, "SELECT COUNT(*) FROM notes WHERE status != 'void'");
        ok = st.ok() && st.step() == SQLITE_ROW && st.int64(0) == 0;
    }
    ok = ok && exec(db_, "DELETE FROM notes", &err);
    if (!ok || !exec(db_, "COMMIT", &err)) {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return false;
    }
    return true;
}

bool NoteStore::debug_exec(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    return exec(db_, sql.c_str(), &err);
}

// ---- conflicts -----------------------------------------------------------------------------------

int NoteStore::fault_authorizer(void* user, int action, const char*, const char*, const char*, const char*) {
    auto* self = static_cast<NoteStore*>(user);
    if (action == SQLITE_INSERT || action == SQLITE_UPDATE || action == SQLITE_DELETE) {
        if (++self->write_counter_ == self->fail_nth_write_) return SQLITE_DENY;
    }
    return SQLITE_OK;
}

void NoteStore::test_fail_nth_write(int n) {
    std::lock_guard<std::mutex> lock(mutex_);
    fail_nth_write_ = n;
    write_counter_ = 0;
    sqlite3_set_authorizer(db_, n > 0 ? &NoteStore::fault_authorizer : nullptr, n > 0 ? this : nullptr);
}

bool NoteStore::restore_from_server(int64_t local_id, const ApiNote& server) {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_,
            "UPDATE notes SET status='void',title=?1,content=?2,category=?3,favorite=?4,modified=?5,etag=?6,excerpt=?7 "
            "WHERE local_id=?8 AND status='deleted'");
    if (!st.ok()) return false;
    st.bind_text(1, title_for(server));
    st.bind_text(2, server.content);
    st.bind_text(3, server.category);
    st.bind_int64(4, server.favorite ? 1 : 0);
    st.bind_int64(5, server.modified);
    st.bind_text(6, server.etag);
    st.bind_text(7, make_excerpt(server.content, title_for(server)));
    st.bind_int64(8, local_id);
    return st.step() == SQLITE_DONE && sqlite3_changes(db_) == 1;
}

bool NoteStore::unused_conflict_title_locked(const std::string& base_title, int64_t now, std::string* out) {
    for (int number = 1; number <= 10001; ++number) {
        const std::string candidate = base_title + conflict_suffix(now, number);
        Stmt taken(db_, "SELECT COUNT(*) FROM notes WHERE title=?1");
        if (!taken.ok()) return false;
        taken.bind_text(1, candidate);
        if (taken.step() != SQLITE_ROW) return false;
        if (taken.int64(0) == 0) {
            *out = candidate;
            return true;
        }
    }
    return false;
}

ConflictResolution NoteStore::resolve_conflict(int64_t local_id, const ApiNote& server, int64_t now) {
    ConflictResolution out;
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return out;

    auto rollback = [&]() {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return ConflictResolution();
    };

    // 1. read the CURRENT local row inside the transaction, so an edit made meanwhile is included
    Note local;
    {
        Stmt st(db_, (std::string("SELECT ") + kNoteColumns + " FROM notes WHERE local_id=?1").c_str());
        if (!st.ok()) return rollback();
        st.bind_int64(1, local_id);
        if (st.step() != SQLITE_ROW) return rollback();
        local = read_note(st);
    }
    if (local.status != NoteStatus::Edited) return rollback();

    // the title is part of what can differ: a title-only edit must never be adopted away silently
    const bool identical = local.title == title_for(server) && local.content == server.content &&
                           local.category == server.category && local.favorite == server.favorite;
    if (!identical) {
        // 2. pick an unused conflict title ("... (conflict DATE)", then " #2", " #3", ...)
        std::string copy_title;
        if (!unused_conflict_title_locked(local.title, now, &copy_title)) return rollback();
        // the text is copied as it is

        // 3. insert the copy FIRST
        int64_t copy_id = 0;
        {
            Stmt ins(db_,
                     "INSERT INTO notes(status,title,content,category,favorite,modified,excerpt) "
                     "VALUES('edited',?1,?2,?3,?4,?5,?6)");
            if (!ins.ok()) return rollback();
            ins.bind_text(1, copy_title);
            ins.bind_text(2, local.content);
            ins.bind_text(3, local.category);
            ins.bind_int64(4, local.favorite ? 1 : 0);
            ins.bind_int64(5, now);
            ins.bind_text(6, make_excerpt(local.content, local.title));
            if (ins.step() != SQLITE_DONE) return rollback();
            copy_id = sqlite3_last_insert_rowid(db_);
        }
        // 4. verify what was written against the local text before the original is touched
        {
            Stmt chk(db_, "SELECT title,content FROM notes WHERE local_id=?1");
            if (!chk.ok()) return rollback();
            chk.bind_int64(1, copy_id);
            if (chk.step() != SQLITE_ROW || chk.text(0) != copy_title || chk.text(1) != local.content) return rollback();
        }
        // attachments: pending ones move to the copy, downloaded ones are duplicated for it (research R-4)
        if (!copy_attachments_for_conflict_locked(local_id, copy_id, local, now)) return rollback();
        out.created_copy = true;
        out.copy_local_id = copy_id;
    }

    // 5. only now replace the original with the server's version (status void)
    {
        Stmt up(db_,
                "UPDATE notes SET status='void',title=?1,content=?2,category=?3,favorite=?4,modified=?5,etag=?6,excerpt=?7 "
                "WHERE local_id=?8");
        if (!up.ok()) return rollback();
        up.bind_text(1, title_for(server));
        up.bind_text(2, server.content);
        up.bind_text(3, server.category);
        up.bind_int64(4, server.favorite ? 1 : 0);
        up.bind_int64(5, server.modified);
        up.bind_text(6, server.etag);
        up.bind_text(7, make_excerpt(server.content, title_for(server)));
        up.bind_int64(8, local_id);
        if (up.step() != SQLITE_DONE) return rollback();
    }
    purge_unreferenced_downloads_locked(local_id, server.content);   // what the server text no longer names
    if (!exec(db_, "COMMIT", &err)) return rollback();
    out.ok = true;
    return out;
}

// ---- push support --------------------------------------------------------------------------------

std::vector<Note> NoteStore::pending_notes() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Note> out;
    Stmt st(db_, (std::string("SELECT ") + kNoteColumns + " FROM notes WHERE status != 'void' ORDER BY local_id").c_str());
    while (st.ok() && st.step() == SQLITE_ROW) out.push_back(read_note(st));
    return out;
}

bool NoteStore::confirm_push(int64_t local_id, const ApiNote& server, const PushSnapshot& sent) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return false;
    bool ok = true;
    {
        Stmt cur(db_, "SELECT status,content,category,favorite,title FROM notes WHERE local_id=?1");
        ok = cur.ok();
        if (ok) cur.bind_int64(1, local_id);
        if (ok && cur.step() == SQLITE_ROW) {
            const bool unchanged = cur.text(0) == "edited" && cur.text(1) == sent.content && cur.text(2) == sent.category &&
                                   (cur.int64(3) != 0) == sent.favorite && cur.text(4) == sent.title;
            Stmt up(db_, unchanged
                             ? "UPDATE notes SET status='void', remote_id=COALESCE(remote_id,?1), etag=?2, modified=?3, title=?5 WHERE local_id=?4"
                             : "UPDATE notes SET remote_id=COALESCE(remote_id,?1), etag=?2 WHERE local_id=?4");
            ok = up.ok();
            if (ok) {
                up.bind_int64(1, server.id);
                up.bind_text(2, server.etag);
                up.bind_int64(3, server.modified);
                up.bind_int64(4, local_id);
                if (unchanged) up.bind_text(5, title_for(server));
                ok = up.step() == SQLITE_DONE;
            }
        }
    }
    if (!ok || !exec(db_, "COMMIT", &err)) {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return false;
    }
    return true;
}

bool NoteStore::clear_remote_identity(int64_t local_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_, "UPDATE notes SET remote_id=NULL, etag=NULL WHERE local_id=?1 AND status='edited'");
    if (!st.ok()) return false;
    st.bind_int64(1, local_id);
    return st.step() == SQLITE_DONE;
}

bool NoteStore::remove_note(int64_t local_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_, "DELETE FROM notes WHERE local_id=?1 AND status='deleted'");
    if (!st.ok()) return false;
    st.bind_int64(1, local_id);
    return st.step() == SQLITE_DONE;
}

// ---- sync bookkeeping --------------------------------------------------------------------------

bool NoteStore::set_collection_state(const std::optional<std::string>& etag, int64_t last_modified,
                                     const std::string& api_versions) {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_, "UPDATE account SET collection_etag=?1, collection_last_modified=?2, api_versions=?3 WHERE id=1");
    if (!st.ok()) return false;
    if (etag) st.bind_text(1, *etag); else st.bind_null(1);
    st.bind_int64(2, last_modified);
    st.bind_text(3, api_versions);
    return st.step() == SQLITE_DONE && sqlite3_changes(db_) == 1;
}

bool NoteStore::record_sync_result(const SyncResult& r, int64_t now) {
    if (r.outcome == SyncOutcome::Running) return true;  // transient; never persisted
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_,
            "UPDATE account SET last_sync_at=?1, last_sync_outcome=?2, last_sync_message=?3, last_sync_counts=?4 "
            "WHERE id=1");
    if (!st.ok()) return false;
    st.bind_int64(1, now);
    st.bind_text(2, to_string(r.outcome));
    st.bind_text(3, r.message_key);
    st.bind_text(4, counts_to_json(r.counts));
    return st.step() == SQLITE_DONE && sqlite3_changes(db_) == 1;
}

}  // namespace pbnotes
