// NoteStore: attachments (spec 002, data-model.md). The rows describe files the app knows about; their
// paths come from note text or the server and are validated here, never used to build a local path.
#include <algorithm>
#include <set>

#include "pbnotes/attachment_ref.h"
#include "pbnotes/markdown.h"
#include "pbnotes/note_store.h"
#include "pbnotes/title.h"
#include "store_sql.h"

namespace pbnotes {

using sql::exec;
using sql::Stmt;

namespace {

const char* kAttachmentColumns =
    "id,note_local_id,remote_path,display_name,kind,size,state,error_key,local_file,created";

Attachment read_attachment(const Stmt& st) {
    Attachment a;
    a.id = st.int64(0);
    if (!st.is_null(1)) a.note_local_id = st.int64(1);
    if (!st.is_null(2)) a.remote_path = st.text(2);
    a.display_name = st.text(3);
    a.kind = attachment_kind_from_string(st.text(4)).value_or(AttachmentKind::File);
    if (!st.is_null(5)) a.size = st.int64(5);
    a.state = attachment_state_from_string(st.text(6)).value_or(AttachmentState::Remote);
    a.error_key = st.text(7);
    a.local_file = st.text(8);
    a.created = st.int64(9);
    return a;
}

bool note_exists(sqlite3* db, int64_t local_id) {
    Stmt st(db, "SELECT 1 FROM notes WHERE local_id=?1 AND status != 'deleted'");
    if (!st.ok()) return false;
    st.bind_int64(1, local_id);
    return st.step() == SQLITE_ROW;
}

// "<id>" or "<id>.<ext>" with ext of 1-8 lower-case letters or digits; returns the id (0 = invalid).
int64_t local_file_id(const std::string& name) {
    size_t i = 0;
    int64_t id = 0;
    while (i < name.size() && name[i] >= '0' && name[i] <= '9' && i < 15) id = id * 10 + (name[i++] - '0');
    if (i == 0) return 0;
    if (i == name.size()) return id;
    if (name[i] != '.') return 0;
    const size_t ext = name.size() - i - 1;
    if (ext < 1 || ext > 8) return 0;
    for (size_t k = i + 1; k < name.size(); ++k) {
        const char c = name[k];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return 0;
    }
    return id;
}

}  // namespace

std::optional<Attachment> NoteStore::get_attachment(int64_t id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_, (std::string("SELECT ") + kAttachmentColumns + " FROM attachments WHERE id=?1").c_str());
    if (!st.ok()) return std::nullopt;
    st.bind_int64(1, id);
    if (st.step() != SQLITE_ROW) return std::nullopt;
    return read_attachment(st);
}

std::optional<Attachment> NoteStore::attachment_by_key_locked(int64_t note_local_id, const std::string& remote_path) const {
    Stmt st(db_, (std::string("SELECT ") + kAttachmentColumns + " FROM attachments WHERE note_local_id=?1 AND remote_path=?2").c_str());
    if (!st.ok()) return std::nullopt;
    st.bind_int64(1, note_local_id);
    st.bind_text(2, remote_path);
    if (st.step() != SQLITE_ROW) return std::nullopt;
    return read_attachment(st);
}

std::optional<Attachment> NoteStore::attachment_for(int64_t note_local_id, const std::string& remote_path) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return attachment_by_key_locked(note_local_id, remote_path);
}

std::optional<Attachment> NoteStore::ensure_attachment_locked(int64_t note_local_id, const std::string& remote_path) {
    if (!validate_attachment_path(remote_path)) return std::nullopt;
    if (!note_exists(db_, note_local_id)) return std::nullopt;
    if (auto existing = attachment_by_key_locked(note_local_id, remote_path)) return existing;
    Stmt ins(db_,
             "INSERT INTO attachments(note_local_id,remote_path,display_name,kind,state,created) "
             "VALUES(?1,?2,?3,?4,'remote',CAST(strftime('%s','now') AS INTEGER))");
    if (!ins.ok()) return std::nullopt;
    ins.bind_int64(1, note_local_id);
    ins.bind_text(2, remote_path);
    ins.bind_text(3, attachment_file_name(remote_path));
    ins.bind_text(4, to_string(attachment_kind_for(remote_path)));
    if (ins.step() != SQLITE_DONE) return std::nullopt;
    return attachment_by_key_locked(note_local_id, remote_path);
}

std::optional<Attachment> NoteStore::ensure_attachment(int64_t note_local_id, const std::string& remote_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    return ensure_attachment_locked(note_local_id, remote_path);
}

bool NoteStore::record_size(int64_t note_local_id, const std::string& remote_path, int64_t size) {
    if (size < 0) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return false;
    bool ok = false;
    if (auto row = ensure_attachment_locked(note_local_id, remote_path)) {
        if (row->state == AttachmentState::Remote || size <= kMaxAttachmentBytes) {
            Stmt up(db_, "UPDATE attachments SET size=?1 WHERE id=?2");
            if (up.ok()) {
                up.bind_int64(1, size);
                up.bind_int64(2, row->id);
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

bool NoteStore::record_downloaded(int64_t note_local_id, const std::string& remote_path, int64_t size,
                                  const std::string& local_file) {
    if (size < 0 || size > kMaxAttachmentBytes || local_file_id(local_file) == 0) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return false;
    bool ok = false;
    if (auto row = ensure_attachment_locked(note_local_id, remote_path)) {
        if (local_file_id(local_file) == row->id) {  // the file is named after its row
            Stmt up(db_, "UPDATE attachments SET state='downloaded', size=?1, local_file=?2, error_key='' WHERE id=?3");
            if (up.ok()) {
                up.bind_int64(1, size);
                up.bind_text(2, local_file);
                up.bind_int64(3, row->id);
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

bool NoteStore::set_attachment_error(int64_t id, int64_t note_local_id, const std::string& remote_path,
                                     const std::string& error_key) {
    if (!is_attachment_error_key(error_key)) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return false;
    int64_t row_id = id;
    bool ok = true;
    if (row_id == 0) {
        auto row = ensure_attachment_locked(note_local_id, remote_path);
        ok = row.has_value();
        if (ok) row_id = row->id;
    }
    if (ok) {
        Stmt up(db_, "UPDATE attachments SET error_key=?1 WHERE id=?2");
        ok = up.ok();
        if (ok) {
            up.bind_text(1, error_key);
            up.bind_int64(2, row_id);
            ok = up.step() == SQLITE_DONE && sqlite3_changes(db_) == 1;
        }
    }
    if (!ok || !exec(db_, "COMMIT", &err)) {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return false;
    }
    return true;
}

std::vector<std::string> NoteStore::referenced_local_files() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    Stmt st(db_, "SELECT local_file FROM attachments WHERE local_file != ''");
    while (st.ok() && st.step() == SQLITE_ROW) out.push_back(st.text(0));
    return out;
}

void NoteStore::purge_unreferenced_downloads_locked(int64_t note_local_id, const std::string& content) {
    std::set<std::string> referenced;
    for (const AttachmentRef& ref : find_attachment_refs(content))
        if (!ref.path.empty()) referenced.insert(ref.path);
    std::vector<int64_t> stale;
    {
        Stmt st(db_, "SELECT id, remote_path FROM attachments WHERE note_local_id=?1 AND state='downloaded' AND remote_path IS NOT NULL");
        if (!st.ok()) return;
        st.bind_int64(1, note_local_id);
        while (st.step() == SQLITE_ROW)
            if (!referenced.count(st.text(1))) stale.push_back(st.int64(0));
    }
    for (int64_t id : stale) {
        Stmt del(db_, "DELETE FROM attachments WHERE id=?1");
        if (!del.ok()) return;
        del.bind_int64(1, id);
        del.step();
    }
}

void NoteStore::purge_unreferenced_downloads(int64_t note_local_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt st(db_, "SELECT content FROM notes WHERE local_id=?1");
    if (!st.ok()) return;
    st.bind_int64(1, note_local_id);
    if (st.step() != SQLITE_ROW) return;
    purge_unreferenced_downloads_locked(note_local_id, st.text(0));
}

// ---- adding and uploading ---------------------------------------------------------------------------------

std::optional<Attachment> NoteStore::stage_attachment(const std::string& display_name, int64_t now) {
    const std::string name = attachment_file_name(display_name);
    if (name.empty()) return std::nullopt;
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt ins(db_,
             "INSERT INTO attachments(note_local_id,remote_path,display_name,kind,state,created) "
             "VALUES(NULL,NULL,?1,?2,'staged',?3)");
    if (!ins.ok()) return std::nullopt;
    ins.bind_text(1, name);
    ins.bind_text(2, to_string(attachment_kind_for(name)));
    ins.bind_int64(3, now);
    if (ins.step() != SQLITE_DONE) return std::nullopt;
    const int64_t id = sqlite3_last_insert_rowid(db_);
    Stmt st(db_, (std::string("SELECT ") + kAttachmentColumns + " FROM attachments WHERE id=?1").c_str());
    if (!st.ok()) return std::nullopt;
    st.bind_int64(1, id);
    if (st.step() != SQLITE_ROW) return std::nullopt;
    return read_attachment(st);
}

bool NoteStore::set_staged_file(int64_t id, int64_t size, const std::string& local_file) {
    if (size < 0 || size > kMaxAttachmentBytes || local_file_id(local_file) != id) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt up(db_, "UPDATE attachments SET size=?1, local_file=?2 WHERE id=?3 AND state='staged' AND note_local_id IS NULL");
    if (!up.ok()) return false;
    up.bind_int64(1, size);
    up.bind_text(2, local_file);
    up.bind_int64(3, id);
    return up.step() == SQLITE_DONE && sqlite3_changes(db_) == 1;
}

bool NoteStore::drop_staged(int64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    Stmt del(db_, "DELETE FROM attachments WHERE id=?1 AND state='staged' AND note_local_id IS NULL");
    if (!del.ok()) return false;
    del.bind_int64(1, id);
    return del.step() == SQLITE_DONE && sqlite3_changes(db_) == 1;
}

bool NoteStore::bind_staged_locked(int64_t note_local_id, const std::string& content) {
    for (int64_t id : find_tokens(content)) {
        Stmt up(db_,
                "UPDATE attachments SET note_local_id=?1, state='pending' "
                "WHERE id=?2 AND state='staged' AND note_local_id IS NULL AND local_file != ''");
        if (!up.ok()) return false;
        up.bind_int64(1, note_local_id);
        up.bind_int64(2, id);
        if (up.step() != SQLITE_DONE) return false;
    }
    return true;
}

bool NoteStore::discard_staged() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return false;
    std::vector<int64_t> loose;
    {
        Stmt st(db_, "SELECT id FROM attachments WHERE state='staged' AND note_local_id IS NULL");
        while (st.ok() && st.step() == SQLITE_ROW) loose.push_back(st.int64(0));
    }
    bool ok = true;
    for (int64_t id : loose) {
        // a saved note that names this row (a crash between the save and the binding): bind it after all
        int64_t owner = 0;
        {
            Stmt find(db_, "SELECT local_id, content FROM notes WHERE status != 'deleted' AND content LIKE ?1");
            ok = find.ok();
            if (!ok) break;
            find.bind_text(1, "%pbatt:" + std::to_string(id) + "%");
            while (find.step() == SQLITE_ROW) {
                const std::vector<int64_t> tokens = find_tokens(find.text(1));
                if (std::find(tokens.begin(), tokens.end(), id) != tokens.end()) {
                    owner = find.int64(0);
                    break;
                }
            }
        }
        Stmt change(db_, owner ? "UPDATE attachments SET note_local_id=?1, state='pending' WHERE id=?2 AND local_file != ''"
                               : "DELETE FROM attachments WHERE id=?2");
        ok = change.ok();
        if (!ok) break;
        if (owner) change.bind_int64(1, owner);
        change.bind_int64(2, id);
        ok = change.step() == SQLITE_DONE;
        if (!ok) break;
        if (owner && sqlite3_changes(db_) == 0) {  // a row without a file cannot be bound: drop it
            Stmt del(db_, "DELETE FROM attachments WHERE id=?1");
            ok = del.ok();
            if (ok) {
                del.bind_int64(1, id);
                ok = del.step() == SQLITE_DONE;
            }
            if (!ok) break;
        }
    }
    if (!ok || !exec(db_, "COMMIT", &err)) {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return false;
    }
    return true;
}

std::vector<Attachment> NoteStore::pending_attachments(int64_t note_local_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Attachment> out;
    Stmt st(db_, (std::string("SELECT ") + kAttachmentColumns +
                  " FROM attachments WHERE note_local_id=?1 AND state='pending' ORDER BY id").c_str());
    if (!st.ok()) return out;
    st.bind_int64(1, note_local_id);
    while (st.step() == SQLITE_ROW) out.push_back(read_attachment(st));
    return out;
}

bool NoteStore::drop_dead_tokens(int64_t note_local_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string content, title;
    {
        Stmt cur(db_, "SELECT content,title FROM notes WHERE local_id=?1");
        if (!cur.ok()) return false;
        cur.bind_int64(1, note_local_id);
        if (cur.step() != SQLITE_ROW) return false;
        content = cur.text(0);
        title = cur.text(1);
    }
    const std::vector<int64_t> tokens = find_tokens(content);
    if (tokens.empty()) return true;
    std::set<int64_t> alive;
    {
        Stmt st(db_, "SELECT id FROM attachments WHERE note_local_id=?1 AND state='pending'");
        if (!st.ok()) return false;
        st.bind_int64(1, note_local_id);
        while (st.step() == SQLITE_ROW) alive.insert(st.int64(0));
    }
    std::vector<int64_t> dead;
    for (int64_t id : tokens)
        if (!alive.count(id)) dead.push_back(id);
    if (dead.empty()) return true;
    const std::string updated = strip_tokens(content, dead);
    Stmt up(db_, "UPDATE notes SET content=?1, excerpt=?2 WHERE local_id=?3");
    if (!up.ok()) return false;
    up.bind_text(1, updated);
    up.bind_text(2, make_excerpt(updated, title));
    up.bind_int64(3, note_local_id);
    return up.step() == SQLITE_DONE;
}

bool NoteStore::record_uploaded(int64_t id, const std::string& remote_path, int64_t size) {
    if (size < 0 || size > kMaxAttachmentBytes || !validate_attachment_path(remote_path)) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    std::string err;
    if (!exec(db_, "BEGIN IMMEDIATE", &err)) return false;
    auto rollback = [&]() {
        std::string ignored;
        exec(db_, "ROLLBACK", &ignored);
        return false;
    };
    Attachment row;
    {
        Stmt st(db_, (std::string("SELECT ") + kAttachmentColumns + " FROM attachments WHERE id=?1").c_str());
        if (!st.ok()) return rollback();
        st.bind_int64(1, id);
        if (st.step() != SQLITE_ROW) return rollback();
        row = read_attachment(st);
    }
    if (row.state == AttachmentState::Downloaded && row.remote_path && *row.remote_path == remote_path) {
        exec(db_, "ROLLBACK", &err);  // already done: nothing to change
        return true;
    }
    if (row.state != AttachmentState::Pending || !row.note_local_id) return rollback();
    {
        Stmt up(db_, "UPDATE attachments SET remote_path=?1, size=?2, state='downloaded', error_key='' WHERE id=?3");
        if (!up.ok()) return rollback();
        up.bind_text(1, remote_path);
        up.bind_int64(2, size);
        up.bind_int64(3, id);
        if (up.step() != SQLITE_DONE) return rollback();
    }
    {
        Stmt cur(db_, "SELECT content,title FROM notes WHERE local_id=?1");
        if (!cur.ok()) return rollback();
        cur.bind_int64(1, *row.note_local_id);
        if (cur.step() != SQLITE_ROW) return rollback();
        const std::string content = cur.text(0);
        const std::string title = cur.text(1);
        const std::string updated = replace_token(content, id, remote_path);
        if (updated != content) {
            Stmt up(db_, "UPDATE notes SET content=?1, excerpt=?2 WHERE local_id=?3");
            if (!up.ok()) return rollback();
            up.bind_text(1, updated);
            up.bind_text(2, make_excerpt(updated, title));
            up.bind_int64(3, *row.note_local_id);
            if (up.step() != SQLITE_DONE) return rollback();
        }
    }
    if (!exec(db_, "COMMIT", &err)) return rollback();
    return true;
}

// ---- conflicts (research R-4) ------------------------------------------------------------------------------

bool NoteStore::copy_attachments_for_conflict_locked(int64_t original, int64_t copy, const Note& local, int64_t now) {
    // pending attachments follow the local text to the copy
    {
        Stmt mv(db_, "UPDATE attachments SET note_local_id=?1 WHERE note_local_id=?2 AND state='pending'");
        if (!mv.ok()) return false;
        mv.bind_int64(1, copy);
        mv.bind_int64(2, original);
        if (mv.step() != SQLITE_DONE) return false;
    }
    // downloaded attachments the local text names get a duplicate row for the copy (the file is shared)
    std::set<std::string> referenced;
    for (const AttachmentRef& ref : find_attachment_refs(local.content))
        if (!ref.path.empty()) referenced.insert(ref.path);
    std::map<std::string, int64_t> token_for_path;
    std::vector<Attachment> sources;
    {
        Stmt st(db_, (std::string("SELECT ") + kAttachmentColumns +
                      " FROM attachments WHERE note_local_id=?1 AND state='downloaded' AND remote_path IS NOT NULL ORDER BY id").c_str());
        if (!st.ok()) return false;
        st.bind_int64(1, original);
        while (st.step() == SQLITE_ROW) {
            Attachment a = read_attachment(st);
            if (referenced.count(*a.remote_path) && !a.local_file.empty()) sources.push_back(std::move(a));
        }
    }
    for (const Attachment& a : sources) {
        Stmt ins(db_,
                 "INSERT INTO attachments(note_local_id,remote_path,display_name,kind,size,state,local_file,created) "
                 "VALUES(?1,NULL,?2,?3,?4,'pending',?5,?6)");
        if (!ins.ok()) return false;
        ins.bind_int64(1, copy);
        ins.bind_text(2, a.display_name);
        ins.bind_text(3, to_string(a.kind));
        if (a.size) ins.bind_int64(4, *a.size); else ins.bind_null(4);
        ins.bind_text(5, a.local_file);
        ins.bind_int64(6, now);
        if (ins.step() != SQLITE_DONE) return false;
        token_for_path[*a.remote_path] = sqlite3_last_insert_rowid(db_);
    }
    if (token_for_path.empty()) return true;
    Stmt cur(db_, "SELECT content FROM notes WHERE local_id=?1");
    if (!cur.ok()) return false;
    cur.bind_int64(1, copy);
    if (cur.step() != SQLITE_ROW) return false;
    const std::string content = cur.text(0);
    const std::string updated = tokenize_attachment_refs(content, token_for_path);
    if (updated == content) return true;
    Stmt up(db_, "UPDATE notes SET content=?1, excerpt=?2 WHERE local_id=?3");
    if (!up.ok()) return false;
    up.bind_text(1, updated);
    up.bind_text(2, make_excerpt(updated, local.title));   // the copy's title carries the marker; the preview follows the original
    up.bind_int64(3, copy);
    return up.step() == SQLITE_DONE;
}

}  // namespace pbnotes
