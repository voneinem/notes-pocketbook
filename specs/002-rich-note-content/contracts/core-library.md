# Contract: core library additions (Qt-free, host-tested)

Extends `specs/001-pocketbook-notes-mvp/contracts/core-library.md`. Everything here lives in
`notes_core` — no Qt, no InkView (constitution V). Names are the intended public surface; tests
are written against it first (constitution I).

## utf8.h (new)

```cpp
// End of the display unit that starts at byte `pos` of valid UTF-8 `text` (pos must be at a code
// point start): the code point plus following variation selectors (U+FE0E/FE0F), emoji modifiers
// (U+1F3FB..1F3FF), combining marks and U+20E3; parts joined by U+200D; a pair of regional
// indicators; tag sequences (U+E0020..E007F). Invalid bytes count as one unit each. Never returns
// pos or less for pos < size.
size_t utf8_cluster_end(std::string_view text, size_t pos);
```

`make_excerpt` (title.h) uses it: it stops only at unit boundaries, so an excerpt may exceed
`max_chars` by the length of one unit.

## links.h (new)

```cpp
bool is_followable_url(std::string_view url);
// http:// or https:// (scheme case-insensitive), non-empty host, no whitespace or control chars,
// at most 2048 bytes.

bool link_needs_confirmation(std::string_view text, std::string_view url);
// false iff `text` is itself a web address naming the same destination as `url` after ignoring the
// scheme, a leading "www.", the case of the host and one trailing "/". True otherwise (spec FR-002).
```

## markdown.h (changed)

```cpp
struct Span {
    enum Kind { Plain, Bold, Italic, BoldItalic };
    Kind kind = Plain;
    std::string text;
    std::string url;               // non-empty = link (validated by is_followable_url)
    bool link_confirm = false;     // link_needs_confirmation(text, url)
};

struct AttachmentRef {             // an attachment reference found in the text
    std::string name;              // alt text / link text (may be empty)
    std::string path;              // validated relative path (decoded), or empty for a token
    int64_t token_id = 0;          // pbatt:<id>, else 0
    bool is_image_syntax = false;  // ![..](..) vs [..](..)
};

enum class BlockKind { Heading, Paragraph, ListItem, Blank, Table, Attachment };

struct TableData {
    enum Align { None, Left, Center, Right };
    std::vector<Align> align;                              // per column
    std::vector<double> weight;                            // per column, > 0, sums to 1
    std::vector<std::vector<std::vector<Span>>> rows;      // rows[0] is the header
};

struct Block {                     // existing fields unchanged, plus:
    int line_count = 1;            // source lines covered (Table: header + delimiter + rows)
    TableData table;               // BlockKind::Table
    AttachmentRef attachment;      // BlockKind::Attachment
};
```

Parsing rules (full list in research R-1, R-9, R-10):

- Links: `[text](url)` (text keeps emphasis), bare `http(s)://…` addresses and `<http(s)://…>`;
  non-followable targets render as their label exactly as today. External images
  `![alt](http(s)://…)` become a link whose text is `alt` (or the address if empty).
- Attachments: a valid relative reference (R-1/data-model) ends the current text segment; the
  line becomes paragraph(s) and Attachment block(s) in source order, all with the same
  `line_index`. Inside a table cell an attachment is a plain-text span with its name.
- Tables: only at top level, outside code fences, lists and quotes; invalid delimiter row ⇒ plain
  paragraphs. Blocks after a table continue at `line_index + line_count`.
- `toggle_task_item` is unchanged and never matches inside a table.

## attachment_ref.h (new)

```cpp
std::optional<std::string> validate_attachment_path(std::string_view decoded_path);   // data-model rules
std::string encode_attachment_path(std::string_view path);        // per-segment percent-encoding
AttachmentKind attachment_kind_for(std::string_view file_name);   // Image | File, by extension
std::string safe_extension(std::string_view file_name);           // [a-z0-9]{1,8} or ""
std::string make_token(int64_t id, std::string_view name);        // "![name](pbatt:<id>)"
std::vector<int64_t> find_tokens(std::string_view content);
std::string replace_token(std::string_view content, int64_t id, std::string_view remote_path);
std::string strip_tokens(std::string_view content, const std::vector<int64_t>& ids);
```

## types.h (changed)

```cpp
enum class AttachmentKind { Image, File };
enum class AttachmentState { Staged, Pending, Remote, Downloaded };
struct Attachment { int64_t id; std::optional<int64_t> note_local_id; std::optional<std::string> remote_path;
                    std::string display_name; AttachmentKind kind; std::optional<int64_t> size;
                    AttachmentState state; std::string error_key; std::string local_file; int64_t created; };
struct SyncCounts { /* existing */ int attachments_uploaded = 0; int attachments_failed = 0; };
constexpr int64_t kMaxAttachmentBytes = 10 * 1024 * 1024;
```

## paging.h (new)

```cpp
struct Interval { double top, bottom; };   // content coordinates of an unsplittable block
// Scroll offset of the next page. `step` is the plain page distance. If an unsplittable block straddles
// the new edge, the edge moves to that block's top (forward: the next page starts at it; backward: the
// new top moves up to it). Blocks taller than the view are ignored. The result is clamped to
// [0, content_height - view_height] and always differs from `content_y` unless already at the start/end.
double page_target(double content_y, double view_height, double content_height, double step,
                   int direction, const std::vector<Interval>& unsplittable);
```

## http_transport.h (changed)

`Request` gains `upload_file` (path, field name, file name), `save_body_to` (path), `max_body_bytes`
(0 = no limit) and accepts `method = "HEAD"`. `Response` gains `body_too_large` (set when the cap
stopped the transfer; `status` is still the server's). When `save_body_to` is set, `body` stays empty.
Credential, TLS, protocol and redirect behavior is unchanged and covered for these methods by the
existing tests, extended.

## api_client.h (changed)

```cpp
bool supports_attachments(const std::string& api_versions);          // contains "1.4"

struct AttachmentUploadResult { ErrorClass error; long status; bool too_large, rejected, unsupported;
                                std::string remote_path; std::string detail; };
struct AttachmentFetchResult  { ErrorClass error; long status; bool not_found, too_large, unsupported, rejected;
                                std::optional<int64_t> size; std::string detail; };
struct AttachmentProbeResult  { ErrorClass error; long status; bool not_found; std::optional<int64_t> size; };

AttachmentUploadResult upload_attachment(int64_t note_remote_id, const std::string& file_path,
                                         const std::string& display_name);
AttachmentFetchResult  download_attachment(int64_t note_remote_id, const std::string& remote_path,
                                           const std::string& dest_part_path, int64_t max_bytes);
AttachmentProbeResult  probe_attachment(int64_t note_remote_id, const std::string& remote_path);
```

## note_store.h (changed)

```cpp
static constexpr int kSchemaVersion = 2;

// staging and binding
std::optional<Attachment> stage_attachment(const std::string& display_name, AttachmentKind kind,
                                           int64_t size, const std::string& local_file, int64_t now);
// create_local / save_local bind the staged attachments whose token appears in the saved content to the
// note IN THE SAME TRANSACTION as the save (no separate bind step; a crash can never leave a saved
// token pointing at an unbound row).
bool discard_staged();   // drops staged rows that no note's content references (a referenced one is bound
                         // instead); files are removed by the janitor
// queries
std::optional<Attachment> get_attachment(int64_t id) const;
std::optional<Attachment> attachment_for(int64_t note_local_id, const std::string& remote_path) const;
std::vector<Attachment> pending_attachments(int64_t note_local_id) const;   // state Pending
// transitions
bool record_uploaded(int64_t id, const std::string& remote_path, int64_t size);   // see data-model
bool record_downloaded(int64_t note_local_id, const std::string& remote_path, const std::string& display_name,
                       AttachmentKind kind, int64_t size, const std::string& local_file);
bool record_size(int64_t note_local_id, const std::string& remote_path, int64_t size);   // creates a Remote row if needed
bool set_attachment_error(int64_t id_or_0, int64_t note_local_id, const std::string& remote_path,
                          const std::string& error_key);
std::vector<std::string> referenced_local_files() const;                 // for the janitor
void purge_unreferenced_downloads(int64_t note_local_id);                // after a pull changed the text
```

Existing operations keep their signatures. `resolve_conflict` additionally re-binds or duplicates
attachment rows for the copy in its existing transaction (research R-4). `apply_remote` calls
`purge_unreferenced_downloads` for each note whose text changed.

## attachment_service.h (new)

Blocking, called from worker threads by the bridge (same pattern as sync). Owns no threads.

```cpp
struct AddResult { bool ok; int64_t id; std::string token; std::string display_name; int64_t size; std::string error_key; };
class AttachmentService {
public:
    AttachmentService(NoteStore&, HttpTransport&, std::string attachments_dir, Clock = nullptr);
    AddResult add(const std::string& source_path);    // copy to <id>.part -> rename; stage; never touches the source
    struct DownloadResult { bool ok; std::string error_key; ErrorClass transport_error; };
    DownloadResult download(int64_t note_local_id, const std::string& remote_path);
    std::optional<int64_t> probe_size(int64_t note_local_id, const std::string& remote_path);
    void purge_files();                                // the janitor
};
```

## sync_engine (changed)

Per pending note, in order: create-without-tokens if it has no server id and has pending
attachments → upload pending attachments (`record_uploaded` after each) → PUT/POST the note with
tokens that could not be uploaded removed from the outgoing text only. The run never downloads
attachments. `SyncCounts` reports uploads and failures; any failure makes the outcome `Partial`
with message key `attachments_failed`.
