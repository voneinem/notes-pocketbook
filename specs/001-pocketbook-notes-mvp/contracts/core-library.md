# Contract: `notes_core` public interfaces

Qt-free C++17 static library (`namespace pbnotes`). Everything here is compiled and tested on the
host without Qt, InkView, or the SDK. Signatures are illustrative contracts; exact names are fixed
by the tests written first (Test-First).

## Types

- `Note` { local_id, remote_id?, status (`Void|Edited|Deleted`), title, content, category,
  favorite, modified, etag, excerpt } — see `data-model.md`.
- `Account` { server_url, username, password, sync state fields }.
- `SyncOutcome` = `Never | Running | Success | Partial | Failed | Offline | Maintenance`.
- `SyncResult` { outcome, message_key, pushed, pulled, deleted, conflicts, recreated, error_class }.
- `ErrorClass` = `None | Network | Tls | Auth | Maintenance | Unsupported | Server`.

## HttpTransport (abstract; mocked in tests)

```text
Response perform(const Request&)      // Request{method,url,headers,body,timeouts}; Response{status,headers,body,error}
```
- MUST refuse non-`https` URLs when the production transport is used (the fake transport used in
  tests accepts any URL so fixtures can run without TLS).
- `CurlTransport(CaSource)` implements it. Verification flags are not configurable.

## CaSelector

```text
CaSelection select_ca_file(const std::vector<std::string>& device_candidates, const std::string& bundled_path)
EffectiveCa build_effective_ca(const std::string& base_ca_path, const std::string& custom_ca_path, const std::string& out_path)
// -> { path, custom_status: None|Loaded|Invalid, custom_cert_count }; base + custom-ca.pem concatenated; verification is never disabled
// -> { path, source: Device|Bundled }, never "none"
```
- A candidate is usable if it exists, is readable, is non-empty and contains a PEM certificate marker.

## ApiClient (Notes API v1)

```text
Capabilities fetch_capabilities();                       // versions, requires 1.x
ListResult   list_notes(optional<string> etag, int64 prune_before);
Note         get_note(int64 id, optional<string> if_none_match);
Note         create_note(const NoteDraft&);              // POST
UpdateResult update_note(int64 id, const NoteDraft&, const string& if_match); // PUT, 412 surfaced
void         delete_note(int64 id, optional<string> if_match);
```
Returns typed results or `ApiError{class, status}`; never throws across the library boundary for
expected failures. Behavior per `notes-api-v1.md`.

## NoteStore (SQLite)

```text
open(path)                         // pragmas, migrations, integrity check, backup before migration
Account/Settings get/set
create_local(content, category, favorite) -> local_id | rejected(empty)
save_local(local_id, content, category, favorite)    // sets status Edited, new modified, title, excerpt
mark_deleted(local_id)                               // Deleted, or remove row if never synced
list(view, sort, query) -> vector<NoteSummary>       // views: All, Favorites, Uncategorized, Category(name)
categories() -> vector<{name,count}>
get(local_id) -> Note
apply_remote(...), confirm_push(local_id, response, sent_snapshot), create_conflict_copy(...)
```
- All mutating calls are atomic. `confirm_push` keeps status `Edited` if the current fields differ
  from `sent_snapshot` (FR-036).

## SyncEngine

```text
SyncResult run(ProgressSink*)      // blocking; push first, then pull; single-flight guard
bool       is_running() const
```
- Returns `AlreadyRunning` immediately if a run is active.
- Updates the account's `last_sync_*` fields before returning.
- Conflict handling, 404 re-creation, delete re-check and non-error statuses as in `research.md` R-8.

## Markdown

```text
vector<Block> parse_markdown(string_view text);          // Heading(level), Paragraph, ListItem(ordered?, checked?, depth), ...
string        toggle_task_item(string_view text, int line_index);   // flips "[ ]" <-> "[x]" in that line only
string        derive_title(string_view text);            // first non-empty line, trimmed
```
- Spans: `Plain`, `Bold`, `Italic`; links become plain text of the link label.
- `toggle_task_item` changes exactly one line and leaves every other byte untouched.

## Test obligations (Test-First, host)

Unit: title, markdown (incl. toggle), CA selection, store CRUD/search/views/sort, state machine.
Contract: API client against `tests/fixtures/*.json` for every operation and status in
`notes-api-v1.md`. Integration: sync engine + store + scripted/fake server for push order, 304/503/
offline, 404 re-creation, 412 conflict copy (also repeated conflicts), delete-vs-edit, interrupted
sync, single-flight, concurrent local edit during push. Performance: 1,000-note search benchmark.
