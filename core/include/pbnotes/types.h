#pragma once
// Plain data types shared by the core library (no Qt, no InkView).

#include <cstdint>
#include <optional>
#include <string>

namespace pbnotes {

// Sync state of a note (data-model.md). `Void` = in sync with the server.
enum class NoteStatus { Void, Edited, Deleted };

struct Note {
    int64_t local_id = 0;
    std::optional<int64_t> remote_id;  // null = never uploaded
    NoteStatus status = NoteStatus::Void;
    std::string title;      // always derive_title(content); never taken from the server
    std::string content;
    std::string category;   // empty = uncategorized
    bool favorite = false;
    int64_t modified = 0;   // epoch seconds
    std::optional<std::string> etag;  // per-note ETag from the server (used as If-Match)
    std::string excerpt;    // derived, never sent
};

// A note as the server describes it (API v1). `pruned` = a listing entry that carries only the id
// (unchanged since the requested pruneBefore time). The server's `title` is deliberately NOT kept:
// the displayed title is always derived locally from the content. The only exception is a note
// without any text: then the server's title (its file name) is shown instead of a blank row.
struct ApiNote {
    int64_t id = 0;
    std::string etag;
    std::string content;
    std::string server_title;  // fallback only: used when the note has no text of its own (file name)
    std::string category;
    bool favorite = false;
    int64_t modified = 0;
    bool readonly = false;
    bool pruned = false;
};

enum class SyncOutcome { Never, Running, Success, Partial, Failed, Offline, Maintenance };

// Classification of a failed request (contracts/notes-api-v1.md).
enum class ErrorClass { None, Network, Tls, Auth, Maintenance, Unsupported, Server };

struct SyncCounts {
    int pushed = 0;
    int pulled = 0;
    int deleted = 0;
    int conflicts = 0;
    int recreated = 0;
    int restored = 0;  // locally deleted notes that were changed on the server and therefore kept
    int attachments_uploaded = 0;  // files sent to the server during this run
    int attachments_failed = 0;    // pending attachments that could not be uploaded (they stay pending)
};

struct SyncResult {
    SyncOutcome outcome = SyncOutcome::Never;
    std::string message_key;  // short machine-readable detail, never note content
    SyncCounts counts;
    ErrorClass error_class = ErrorClass::None;
};

// ---- attachments (spec 002, data-model.md) ----------------------------------------------------------------

// The largest attachment that is added, uploaded or downloaded (10 MiB).
constexpr int64_t kMaxAttachmentBytes = 10 * 1024 * 1024;

enum class AttachmentKind { Image, File };

// staged: added in the editor, not saved with a note yet; pending: saved with a note, not uploaded yet;
// remote: the server has it, nothing downloaded; downloaded: a local copy exists.
enum class AttachmentState { Staged, Pending, Remote, Downloaded };

struct Attachment {
    int64_t id = 0;
    std::optional<int64_t> note_local_id;     // none only while staged
    std::optional<std::string> remote_path;   // path relative to the note as the server knows it (decoded)
    std::string display_name;                 // shown to the user; never used to build a local path
    AttachmentKind kind = AttachmentKind::File;
    std::optional<int64_t> size;              // bytes, none = not known
    AttachmentState state = AttachmentState::Remote;
    std::string error_key;                    // "" or the last failure reason
    std::string local_file;                   // "<id>.<ext>" inside the attachment folder, "" = none
    int64_t created = 0;
};

struct Account {
    std::string server_url;  // https:// only, no trailing slash
    std::string username;
    std::string password;    // Nextcloud app password, stored unencrypted (documented risk)
    std::optional<std::string> collection_etag;
    int64_t collection_last_modified = 0;
    std::string api_versions;  // e.g. "1.0,1.3"
    std::optional<int64_t> last_sync_at;
    SyncOutcome last_sync_outcome = SyncOutcome::Never;
    std::string last_sync_message;
    SyncCounts last_sync_counts;
};

// String forms used in the database and in logs.
const char* to_string(NoteStatus s);
std::optional<NoteStatus> note_status_from_string(const std::string& s);
const char* to_string(SyncOutcome o);
std::optional<SyncOutcome> sync_outcome_from_string(const std::string& s);
const char* to_string(ErrorClass e);
const char* to_string(AttachmentKind k);
std::optional<AttachmentKind> attachment_kind_from_string(const std::string& s);
const char* to_string(AttachmentState s);
std::optional<AttachmentState> attachment_state_from_string(const std::string& s);

// The closed set of attachment failure reasons (Attachment::error_key); "" is "no error".
bool is_attachment_error_key(const std::string& key);

}  // namespace pbnotes
