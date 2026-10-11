#pragma once
// Nextcloud Notes API v1 client (contracts/notes-api-v1.md). Talks only through HttpTransport.
// Never throws for expected failures; every result carries an ErrorClass.

#include <optional>
#include <string>
#include <vector>

#include "pbnotes/http_transport.h"
#include "pbnotes/types.h"

namespace pbnotes {

struct Capabilities {
    std::vector<std::string> notes_api_versions;  // e.g. {"0.2","1.0","1.3"}
    bool has_api_v1() const;
};

struct CapabilitiesResult {
    ErrorClass error = ErrorClass::None;
    long status = 0;
    Capabilities capabilities;
    std::string detail;  // short, never contains credentials or note text
};

struct ListResult {
    ErrorClass error = ErrorClass::None;
    long status = 0;
    bool not_modified = false;       // 304: nothing changed since the validator
    std::vector<ApiNote> notes;      // empty on any error: never a partial result
    std::optional<std::string> etag; // ETag response header of the collection, as sent
    int64_t last_modified = 0;       // Last-Modified header as epoch seconds, 0 if absent/unparsable
    std::string api_versions;        // X-Notes-API-Versions header, raw
    std::string detail;
};

struct NoteResult {
    ErrorClass error = ErrorClass::None;
    long status = 0;
    bool not_modified = false;  // 304 for a conditional GET
    bool not_found = false;     // 404: the note no longer exists on the server
    std::optional<ApiNote> note;
    std::string detail;
};

// What a client sends when creating or updating a note. The title is always sent (spec 004): it is the title the
// user entered, so the server never derives one from the text.
struct NoteDraft {
    std::string title;
    std::string content;
    std::string category;
    bool favorite = false;
    int64_t modified = 0;  // epoch seconds
};

struct WriteResult {
    ErrorClass error = ErrorClass::None;
    long status = 0;
    bool precondition_failed = false;  // 412: the note changed on the server since our validator
    bool not_found = false;            // 404: the note no longer exists on the server
    std::optional<ApiNote> note;       // the server's version after a successful write
    std::string detail;
};

enum class DeleteOutcome {
    Deleted,          // the note was unchanged and has been deleted
    AlreadyGone,      // the server no longer has it (404)
    ChangedOnServer,  // changed since our validator: NOT deleted (a conflict)
    Failed            // see `error`
};

struct DeleteResult {
    DeleteOutcome outcome = DeleteOutcome::Failed;
    ErrorClass error = ErrorClass::None;
    long status = 0;
    std::optional<ApiNote> server_note;  // set when ChangedOnServer was found through the check GET
    std::string detail;
};

// ---- attachments (spec 002, contracts/notes-attachments-api.md) -----------------------------------------

// True if the stored X-Notes-API-Versions list ("1.0, 1.3, 1.4") contains the token "1.4".
bool supports_attachments(const std::string& api_versions);

struct AttachmentFetchResult {
    ErrorClass error = ErrorClass::None;  // a failure of the connection or the account (not of this file)
    long status = 0;
    bool not_found = false;     // 404
    bool too_large = false;     // Content-Length or the body exceeded the limit; nothing was stored
    bool unsupported = false;   // 400: the server has no attachment API
    bool rejected = false;      // any other error status
    bool storage_full = false;  // the file could not be written on this device
    std::optional<int64_t> size;  // bytes stored, on success
    std::string detail;
};

struct AttachmentProbeResult {
    ErrorClass error = ErrorClass::None;
    long status = 0;
    bool not_found = false;
    std::optional<int64_t> size;  // Content-Length of the HEAD answer, if it had one
};

struct AttachmentUploadResult {
    ErrorClass error = ErrorClass::None;  // a failure of the connection or the account
    long status = 0;
    bool too_large = false;     // the file is over the limit (checked before sending) or the server said 413
    bool unreadable = false;    // the local file cannot be read; nothing was sent
    bool unsupported = false;   // 400: the server has no attachment API
    bool not_found = false;     // 404: the note does not exist on the server (any more)
    bool rejected = false;      // any other refusal, or an answer that is not a valid path
    std::string remote_path;    // on success: the path the server stored the file under (validated)
    std::string detail;
};

class ApiClient {
public:
    struct Credentials {
        std::string server_url;  // normalized https:// URL without trailing slash
        std::string username;
        std::string password;
    };

    ApiClient(HttpTransport& transport, Credentials credentials);

    // GET <server>/ocs/v2.php/cloud/capabilities?format=json
    CapabilitiesResult fetch_capabilities();

    // GET <server>/index.php/apps/notes/api/v1/notes[?pruneBefore=<epoch s>] with If-None-Match.
    ListResult list_notes(const std::optional<std::string>& collection_etag, int64_t prune_before);
    // GET .../notes/{id} (optionally conditional with If-None-Match).
    NoteResult get_note(int64_t id, const std::optional<std::string>& if_none_match = std::nullopt);

    // POST .../notes
    WriteResult create_note(const NoteDraft& draft);
    // PUT .../notes/{id} with If-Match: the stored ETag (conditional write, constitution Principle VI).
    WriteResult update_note(int64_t id, const NoteDraft& draft, const std::string& if_match_etag);
    // Deletes only a note that is unchanged on the server. Default: conditional GET with the stored
    // ETag first, then DELETE only on 304 (200 = changed: no DELETE is sent; 404 = already gone).
    // With set_delete_uses_if_match(true) (only once the server is verified to return 412 for a
    // stale If-Match on DELETE) a single DELETE carries If-Match instead.
    DeleteResult delete_note(int64_t id, const std::string& stored_etag);
    void set_delete_uses_if_match(bool enabled) { delete_uses_if_match_ = enabled; }

    // GET .../api/v1.4/attachment/<noteId>?path=<path>: streams the file into `dest_part_path` (written only for
    // a 2xx answer; removed on any failure) and stops at `max_bytes`. No If-Match: attachments are plain files.
    AttachmentFetchResult download_attachment(int64_t note_remote_id, const std::string& remote_path,
                                              const std::string& dest_part_path, int64_t max_bytes);
    // POST .../api/v1.4/attachment/<noteId> as multipart/form-data with the single field `file` (the part's file
    // name is `display_name`), no If-Match. A file over the limit or unreadable makes no request. The answer's
    // `filename` is validated like any reference before it is returned.
    AttachmentUploadResult upload_attachment(int64_t note_remote_id, const std::string& file_path,
                                             const std::string& display_name);
    // HEAD on the same URL with a 3 s timeout: the size if the server announces one (it need not).
    AttachmentProbeResult probe_attachment(int64_t note_remote_id, const std::string& remote_path);

private:
    Request make_request(const std::string& method, const std::string& url) const;
    std::string attachment_url(int64_t note_remote_id, const std::string& remote_path) const;

    WriteResult write_note(const std::string& method, const std::string& url, const NoteDraft& draft,
                           const std::string& if_match_etag);

    HttpTransport& transport_;
    Credentials creds_;
    bool delete_uses_if_match_ = false;
};

// ETag header value for If-Match / If-None-Match: the stored value, wrapped in quotes if it has none.
std::string etag_header_value(const std::string& etag);

// Parses an RFC 1123 date ("Tue, 14 Nov 2023 22:13:20 GMT") to epoch seconds; 0 if unparsable.
int64_t parse_http_date(const std::string& value);

// Maps a response without an HTTP status or with an error status to an ErrorClass
// (401/403 Auth, 503 Maintenance, other 4xx/5xx Server, transport errors passed through).
ErrorClass classify_response(const Response& response);

}  // namespace pbnotes
