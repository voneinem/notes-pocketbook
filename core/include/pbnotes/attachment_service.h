#pragma once
// Attachment file operations (spec 002): download on request, size probing, and the janitor that keeps the
// attachment folder in step with the store. Blocking calls for worker threads (like SyncEngine); the service
// owns no threads. Files are named "<attachment id>.<ext>" only; nothing from a note or the server becomes a
// path. Every write goes to "<id>.part" first and is renamed when complete. No Qt.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "pbnotes/http_transport.h"
#include "pbnotes/note_store.h"
#include "pbnotes/types.h"

namespace pbnotes {

// The result of adding a file in the editor.
struct AddResult {
    bool ok = false;
    int64_t id = 0;             // the staged row; the token in the note text names it
    std::string token;          // "![name](pbatt:<id>)": inserted into the note text at the cursor
    std::string display_name;
    int64_t size = 0;
    std::string error_key;      // unreadable | too_large | storage_full when !ok
};

class AttachmentService {
public:
    using Clock = std::function<int64_t()>;  // epoch seconds

    // `attachments_dir` is created if it does not exist (its parent must).
    AttachmentService(NoteStore& store, HttpTransport& transport, std::string attachments_dir, Clock clock = nullptr);

    struct DownloadResult {
        bool ok = false;
        std::string error_key;                          // set on failure: a key of the closed set in types.h
        ErrorClass transport_error = ErrorClass::None;  // when the connection or the account was the problem
    };
    // Downloads the file a note references (by its path as the server knows it) into the attachment folder and
    // marks the row `downloaded`. A failure is recorded on the row (error_key) and leaves no partial file.
    DownloadResult download(int64_t note_local_id, const std::string& remote_path);

    // Adds a file the user chose: copies it (never moves or changes it) into the attachment folder as
    // "<id>.part", then renames it to "<id>.<ext>" and stages a row (spec 002 FR-025). A file over
    // kMaxAttachmentBytes, an unreadable one or a full device fails with nothing left behind: no part file and
    // no row. The row becomes pending when a saved note carries the token.
    AddResult add(const std::string& source_path);
    // Test hook: the copy fails with "storage full" after this many bytes (-1 = off).
    void test_fail_copy_after(int64_t bytes) { fail_copy_after_ = bytes; }

    // The size of a not-yet-downloaded attachment: the cached one, else a short HEAD request (the server need not
    // answer with a length). A found size is stored. nullopt if unknown.
    std::optional<int64_t> probe_size(int64_t note_local_id, const std::string& remote_path);

    // The janitor: deletes every file this service could have made ("<id>" or "<id>.<ext>") that no row names
    // and every "*.part" file; files with any other name are left alone.
    void purge_files() { purge(store_, dir_); }
    // `min_age_seconds` > 0 spares files written more recently than that: a purge that may overlap a download or
    // an add in progress (a sync running elsewhere) must not remove a file that is just being completed.
    static void purge(NoteStore& store, const std::string& directory, int64_t min_age_seconds = 0);

    const std::string& directory() const { return dir_; }

private:
    std::string path_of(const std::string& file_name) const { return dir_ + "/" + file_name; }

    NoteStore& store_;
    HttpTransport& transport_;
    std::string dir_;
    Clock clock_;
    int64_t fail_copy_after_ = -1;
};

}  // namespace pbnotes
