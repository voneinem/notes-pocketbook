# Contract: core library additions (Qt-free, host-tested)

Extends `specs/001-pocketbook-notes-mvp/contracts/core-library.md`. Everything here lives in
`notes_core` — no Qt, no InkView (constitution V). This file describes the interfaces **as built**
(tests are written against them first, constitution I).

## utf8.h (new)

```cpp
// End of the display unit that starts at byte `pos` of `text` (pos must be at a code point start): the code
// point plus following variation selectors (U+FE0E/FE0F), emoji modifiers (U+1F3FB..1F3FF), combining marks,
// U+20E3 and tag characters (U+E0020..E007F); parts joined by U+200D (only when a character follows it); a
// pair of regional indicators. Every invalid byte is one unit. pos >= size returns size; for pos < size the
// result is always greater than pos.
size_t utf8_cluster_end(std::string_view text, size_t pos);
```

`make_excerpt` (title.h) uses it: it stops only at unit boundaries, so an excerpt may exceed
`max_chars` by the length of one unit. No other code cuts user text by byte or character count.

## links.h (new)

```cpp
bool is_followable_url(std::string_view url);
// http:// or https:// (scheme case-insensitive), non-empty host, no whitespace or control chars, at most 2048 bytes.

bool link_needs_confirmation(std::string_view text, std::string_view url);
// false iff `text` is itself a web address naming the same destination as `url` after ignoring the scheme,
// a leading "www.", the case of the host and one trailing "/". True otherwise (spec FR-002).
```

## markdown.h (changed)

```cpp
struct Span {
    enum Kind { Plain, Bold, Italic, BoldItalic };
    Kind kind = Plain;
    std::string text;
    std::string url;               // non-empty = link (passes is_followable_url)
    bool link_confirm = false;     // link_needs_confirmation(label, url)
};

struct AttachmentRef {             // an attachment reference found in the text
    std::string name;              // alt / link text (may be empty)
    std::string path;              // validated, decoded path; empty for a token
    int64_t token_id = 0;          // pbatt:<id>, else 0
    bool is_image_syntax = false;  // ![..](..) vs [..](..)
};

enum class BlockKind { Heading, Paragraph, ListItem, Blank, Table, Attachment };

struct TableData {
    enum Align { None, Left, Center, Right };
    std::vector<Align> align;                              // per column
    std::vector<double> weight;                            // per column, > 0, sums to 1
    std::vector<std::vector<std::vector<Span>>> rows;      // rows[0] is the header; every row has one cell per column
};

struct Block {                     // existing fields unchanged, plus:
    int line_count = 1;            // source lines covered (Table: header + delimiter + rows)
    TableData table;               // BlockKind::Table
    AttachmentRef attachment;      // BlockKind::Attachment
};

std::vector<AttachmentRef> find_attachment_refs(std::string_view text);   // every reference, incl. table cells
std::string tokenize_attachment_refs(std::string_view text, const std::map<std::string, int64_t>& token_for_path);
// rewrites the references whose decoded path is a key as the local token ![name](pbatt:<id>); all else unchanged
```

Parsing rules (research R-1, R-9, R-10):

- Links: `[text](url)` (text keeps emphasis; an optional `"title"` is ignored; balanced parentheses in the
  address), bare `http(s)://…` addresses (trailing `. , ; : ! ? " '` and an unbalanced `)` excluded) and
  `<http(s)://…>`; non-followable targets render as their label. External images `![alt](http(s)://…)` become a
  link whose text is `alt` (or the address if empty). A link inside a link label is not parsed.
- Attachments: `![name](relative path)`, `[name](.attachments.<…>)` and `![name](pbatt:<id>)` end the current
  text segment; the line becomes text block(s) and Attachment block(s) in source order, all with the same
  `line_index`; the first text keeps the line's kind (heading, list item, task), later text becomes
  paragraphs. Inside a table cell an attachment is a plain-text span with its name. Code fences and inline code
  are never parsed.
- Tables: only at top level, outside code fences, lists, quotes and indented code; the header and the delimiter
  row need the same cell count; an invalid delimiter ⇒ plain paragraphs. Blocks after a table continue at
  `line_index + line_count`. Weights: the longest cell of a column, clamped to 4..40 code points, normalized.
- `toggle_task_item` is unchanged and never matches inside a table.

## attachment_ref.h (new)

```cpp
std::optional<std::string> validate_attachment_path(std::string_view decoded_path);
// non-empty, <= 1024 bytes, no NUL/control char/backslash, no scheme, no leading "/", non-empty last segment;
// ".." segments are allowed (the server confines them)
std::optional<std::string> decode_attachment_target(std::string_view target);
// a reference target as written in the text: percent-decodes each segment (a bad escape or a decoded "/" in a
// segment fails) and validates the result
std::string encode_attachment_path(std::string_view path);        // per-segment percent-encoding, "/" kept
AttachmentKind attachment_kind_for(std::string_view file_name);   // Image for jpg jpeg png gif bmp webp
std::string safe_extension(std::string_view file_name);           // [a-z0-9]{1,8} lower-case, or ""
bool is_viewable_extension(std::string_view extension);           // pdf epub fb2 txt rtf doc docx html htm djvu mobi cbz cbr
std::string attachment_file_name(std::string_view path);          // last segment, control chars removed, <= 255 chars
std::string make_token(int64_t id, std::string_view name);        // "![name](pbatt:<id>)", name sanitized, <= 100 chars
std::vector<int64_t> find_tokens(std::string_view content);
std::string replace_token(std::string_view content, int64_t id, std::string_view remote_path);
std::string strip_tokens(std::string_view content, const std::vector<int64_t>& ids);
// a reference alone on its line takes the following line break with it (on the last line the one before it)
```

## types.h (changed)

```cpp
constexpr int64_t kMaxAttachmentBytes = 10 * 1024 * 1024;
enum class AttachmentKind { Image, File };
enum class AttachmentState { Staged, Pending, Remote, Downloaded };
struct Attachment { int64_t id; std::optional<int64_t> note_local_id; std::optional<std::string> remote_path;
                    std::string display_name; AttachmentKind kind; std::optional<int64_t> size;
                    AttachmentState state; std::string error_key; std::string local_file; int64_t created; };
struct SyncCounts { /* existing */ int attachments_uploaded = 0; int attachments_failed = 0; };
bool is_attachment_error_key(const std::string& key);   // "", too_large, not_found, rejected, unsupported_server,
                                                         // storage_full, unreadable, format_unsupported, network
// plus to_string / *_from_string for kind and state
```

## paging.h (new)

```cpp
struct Interval { double top, bottom; };   // content coordinates of an unsplittable block
// Scroll offset of the next page. `step` is the plain page distance. If an unsplittable block straddles the new
// top edge, the edge moves to that block's top (going down the next page starts at it; going up the new top
// moves up to it). Blocks taller than the view are ignored; a move that would not make progress falls back to
// the plain step. Clamped to [0, content_height - view_height] (0 when the content is shorter than the view).
double page_target(double content_y, double view_height, double content_height, double step,
                   int direction, const std::vector<Interval>& unsplittable);
```

## http_transport.h (changed)

`Request` gains `save_body_to` (a 2xx body is streamed into this file; an error status keeps its small body in
memory and creates no file), `max_body_bytes` (`long long`, 0 = no limit; the transfer stops as soon as
`Content-Length` or the running total exceeds it and the file is removed), and `upload_file` /
`upload_field` (default `file`) / `upload_file_name` (a multipart/form-data body with that one part). `HEAD` is
allowed. `Response` gains `body_too_large` and `storage_full` (writing the file failed with ENOSPC/EDQUOT);
`body` stays empty when the file sink is used. An unreadable `upload_file` is refused before anything is sent.
Credential, TLS, protocol and redirect behavior is unchanged and covered for these methods by the tests.

## api_client.h (changed)

```cpp
bool supports_attachments(const std::string& api_versions);          // contains the token "1.4"

struct AttachmentFetchResult  { ErrorClass error; long status; bool not_found, too_large, unsupported, rejected,
                                storage_full; std::optional<int64_t> size; std::string detail; };
struct AttachmentProbeResult  { ErrorClass error; long status; bool not_found; std::optional<int64_t> size; };
struct AttachmentUploadResult { ErrorClass error; long status; bool too_large, unreadable, unsupported, not_found,
                                rejected; std::string remote_path; std::string detail; };

AttachmentFetchResult  ApiClient::download_attachment(int64_t note_remote_id, const std::string& remote_path,
                                                      const std::string& dest_part_path, int64_t max_bytes);
AttachmentProbeResult  ApiClient::probe_attachment(int64_t note_remote_id, const std::string& remote_path);
AttachmentUploadResult ApiClient::upload_attachment(int64_t note_remote_id, const std::string& file_path,
                                                    const std::string& display_name);
```

The query value of `path` is percent-encoded as a whole (`/` too), like `encodeURIComponent`.

## note_store.h (changed)

```cpp
static constexpr int kSchemaVersion = 2;

// queries
std::optional<Attachment> get_attachment(int64_t id) const;
std::optional<Attachment> attachment_for(int64_t note_local_id, const std::string& remote_path) const;
std::vector<Attachment> pending_attachments(int64_t note_local_id) const;      // state Pending, oldest first
std::vector<std::string> referenced_local_files() const;                       // every row's local_file (janitor)

// download side
std::optional<Attachment> ensure_attachment(int64_t note_local_id, const std::string& remote_path);   // Remote row
bool record_size(int64_t note_local_id, const std::string& remote_path, int64_t size);
bool record_downloaded(int64_t note_local_id, const std::string& remote_path, int64_t size, const std::string& local_file);
bool set_attachment_error(int64_t id, int64_t note_local_id, const std::string& remote_path, const std::string& error_key);
void purge_unreferenced_downloads(int64_t note_local_id);

// add / upload side
std::optional<Attachment> stage_attachment(const std::string& display_name, int64_t now);   // row first, id names the file
bool set_staged_file(int64_t id, int64_t size, const std::string& local_file);
bool drop_staged(int64_t id);
bool discard_staged();          // drops staged rows no note names; binds one a saved note does name
bool drop_dead_tokens(int64_t note_local_id);   // removes tokens no pending attachment answers to
bool record_uploaded(int64_t id, const std::string& remote_path, int64_t size);   // one transaction, see data-model
```

`create_local` / `save_local` bind the staged attachments whose token appears in the saved text to the note IN
THE SAME TRANSACTION as the save (no separate bind step; a crash can never leave a saved token pointing at an
unbound row). `record_downloaded` accepts only `local_file` = `<row id>` or `<row id>.<[a-z0-9]{1,8}>`; rows made
for a conflict copy share the source row's file name instead (data-model.md). `resolve_conflict` additionally
moves pending attachments to the copy and duplicates the downloaded ones its text names (research R-4) in its
existing transaction, and removes `downloaded` rows the server text no longer names. `apply_remote` calls the
purge for every note whose text changed.

## attachment_service.h (new)

Blocking, called from worker threads by the bridge (same pattern as sync). Owns no threads.

```cpp
struct AddResult { bool ok; int64_t id; std::string token; std::string display_name; int64_t size; std::string error_key; };
class AttachmentService {
public:
    AttachmentService(NoteStore&, HttpTransport&, std::string attachments_dir, Clock = nullptr);
    AddResult add(const std::string& source_path);   // copy to <id>.part -> rename; stage; never touches the source
    struct DownloadResult { bool ok; std::string error_key; ErrorClass transport_error; };
    DownloadResult download(int64_t note_local_id, const std::string& remote_path);
    std::optional<int64_t> probe_size(int64_t note_local_id, const std::string& remote_path);
    void purge_files();                                                         // the janitor
    static void purge(NoteStore&, const std::string& dir, int64_t min_age_seconds = 0);
    void test_fail_copy_after(int64_t bytes);                                   // test hook: "storage full" after n bytes
};
```

`purge` deletes every plain file no row names and every `*.part`; a positive `min_age_seconds` spares files
written more recently (a purge that may overlap a download or an add in progress).

## sync_engine (changed)

`SyncOptions::attachments_dir` is the folder of the attachment files; without it pending attachments stay
pending with the reason `unreadable`. Per pending note, in order: create-without-tokens if it has no server id
and pending attachments → upload pending attachments (`record_uploaded` after each; a 404 means the note
vanished: it is created again and the uploads repeat once) → PUT/POST the note with the tokens that could not be
resolved removed from the outgoing text only. The run never downloads attachments. `SyncCounts` reports uploads
and failures; any failure makes the outcome `Partial` with message key `attachments_failed`.
