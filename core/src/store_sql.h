#pragma once
// Private to the store implementation (note_store.cpp, note_store_attachments.cpp): thin RAII helpers
// around the SQLite C API. Not part of the public API.

#include <sqlite3.h>

#include <string>

namespace pbnotes {
namespace sql {

inline std::string sqlite_error(sqlite3* db) { return db ? sqlite3_errmsg(db) : "sqlite error"; }

inline bool exec(sqlite3* db, const char* sql, std::string* error) {
    char* msg = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &msg) != SQLITE_OK) {
        if (error) *error = msg ? msg : sqlite_error(db);
        sqlite3_free(msg);
        return false;
    }
    return true;
}

// First column of the first row as text ("" when none).
inline bool query_text(sqlite3* db, const std::string& sql, std::string* out, std::string* error) {
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr) != SQLITE_OK) {
        if (error) *error = sqlite_error(db);
        return false;
    }
    out->clear();
    int rc = sqlite3_step(st);
    if (rc == SQLITE_ROW) {
        const unsigned char* t = sqlite3_column_text(st, 0);
        if (t) *out = reinterpret_cast<const char*>(t);
        rc = SQLITE_OK;
    } else if (rc == SQLITE_DONE) {
        rc = SQLITE_OK;
    } else if (error) {
        *error = sqlite_error(db);
    }
    sqlite3_finalize(st);
    return rc == SQLITE_OK;
}


class Stmt {
public:
    Stmt(sqlite3* db, const char* sql) { ok_ = sqlite3_prepare_v2(db, sql, -1, &st_, nullptr) == SQLITE_OK; }
    ~Stmt() { sqlite3_finalize(st_); }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;
    bool ok() const { return ok_; }
    sqlite3_stmt* get() const { return st_; }
    void bind_text(int i, const std::string& v) { sqlite3_bind_text(st_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT); }
    void bind_int64(int i, long long v) { sqlite3_bind_int64(st_, i, v); }
    void bind_null(int i) { sqlite3_bind_null(st_, i); }
    int step() { return sqlite3_step(st_); }
    std::string text(int col) const {
        const unsigned char* t = sqlite3_column_text(st_, col);
        return t ? std::string(reinterpret_cast<const char*>(t), static_cast<size_t>(sqlite3_column_bytes(st_, col))) : std::string();
    }
    bool is_null(int col) const { return sqlite3_column_type(st_, col) == SQLITE_NULL; }
    long long int64(int col) const { return sqlite3_column_int64(st_, col); }

private:
    sqlite3_stmt* st_ = nullptr;
    bool ok_ = false;
};

}  // namespace sql
}  // namespace pbnotes
