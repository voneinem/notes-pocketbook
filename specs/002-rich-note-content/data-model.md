# Data Model: Rich Note Content

Extends `specs/001-pocketbook-notes-mvp/data-model.md`. The `notes` table and the note state
machine (`void` / `edited` / `deleted`) are unchanged: links, tables and emojis are not stored
anywhere — they are views of the note text. Only attachments add persistent data.

## Schema change: version 1 → 2

One new table. `NoteStore::kSchemaVersion` becomes 2. The existing migration rules apply: a backup
`<db>.bak` is made before migrating, an integrity check follows, and the backup is deleted once both
succeed. A database written by this version is refused by the MVP build (newer schema), as before.

```sql
CREATE TABLE attachments (
    id            INTEGER PRIMARY KEY,
    note_local_id INTEGER REFERENCES notes(local_id) ON DELETE CASCADE,  -- NULL while staged
    remote_path   TEXT,                -- path relative to the note as the server knows it (decoded);
                                       -- NULL until uploaded
    display_name  TEXT NOT NULL,       -- file name shown to the user
    kind          TEXT NOT NULL,       -- 'image' | 'file' (decided from the extension)
    size          INTEGER,             -- bytes; NULL = not known yet
    state         TEXT NOT NULL,       -- staged | pending | remote | downloaded
    error_key     TEXT NOT NULL DEFAULT '',  -- '' or the last failure reason (see below)
    local_file    TEXT NOT NULL DEFAULT '',  -- file name inside <data dir>/attachments/; '' = none
    created       INTEGER NOT NULL
);
CREATE INDEX attachments_note ON attachments(note_local_id);
CREATE UNIQUE INDEX attachments_path ON attachments(note_local_id, remote_path)
    WHERE remote_path IS NOT NULL;
```

`ON DELETE CASCADE` removes the rows when a note is removed (pull deletion, tombstone removal,
`reset_local_data`). The files are removed by a small **janitor** (`purge_attachment_files()`):
it deletes every file in the attachment folder that no row names and every `*.part` file. It runs at
start, after a sync, and after any operation that can orphan files. Rows are deleted first and files
second, so a crash can leave at most an unreferenced file (cleaned up by the janitor), never a row
pointing at a missing file. The user's original file is never touched.

## Attachment (entity)

| Field | Rules |
|-------|-------|
| `id` | Used in the staged token `pbatt:<id>` and as the local file name. |
| `note_local_id` | NULL only in state `staged`. |
| `remote_path` | Validated relative path (see "Reference validation"). Unique per note. NULL for `staged`/`pending`. |
| `display_name` | Last path segment (remote) or the chosen file's name (local), control characters removed, at most 255 characters. Never used to build a local path. |
| `kind` | `image` if the extension is jpg, jpeg, png, gif, bmp or webp (case-insensitive), else `file`. |
| `size` | From the chosen file (add), `HEAD`/`Content-Length` (probe, download). NULL = unknown. Must be ≤ 10 MiB (10,485,760) for `staged`/`pending`/`downloaded` rows. |
| `local_file` | `<id>.<ext>`, ext = lower-case original extension matching `[a-z0-9]{1,8}` or none. |
| `error_key` | `''`, `too_large`, `not_found`, `rejected`, `unsupported_server`, `storage_full`, `unreadable`, `format_unsupported`, `network`. Shown to the user as a plain sentence; never contains names or paths. |

### State machine

```text
(user adds file, editor open)      staged  -- note saved, token in text ----> pending
staged  -- editor left/discarded, token removed, crash cleanup -------------> row + file deleted
pending -- upload ok (remote_path set, token rewritten in the note text) ---> downloaded
pending -- upload failed (error_key set) ------------------------------------> pending (retry next sync)
(reader meets a server reference; row created on first probe/download)       remote
remote  -- download ok (file stored, size set) ------------------------------> downloaded
remote  -- download failed (error_key set) ----------------------------------> remote
downloaded -- reference gone from the note text after a pull, or note removed -> row + file deleted
```

An uploaded attachment keeps its local file (state `downloaded`, `remote_path` set), so it stays
viewable offline without a second download.

## Note text ↔ attachments

### Reference forms in the note text

| Form | Meaning |
|------|---------|
| `![name](<seg>/<seg>…)` relative target | Attachment reference (image syntax). Rendered as image if `kind=image`, else as a file entry. |
| `[name](.attachments.<n>/…)` | Attachment reference (link syntax), file entry or image by extension. |
| `![name](pbatt:<id>)` | **Local token** for a staged or pending attachment. Exists only in locally stored text; never sent to the server (push rewrites or strips it). |
| `![alt](https://…)` | External image: shown as a link with `alt` text; never fetched. |

### Reference validation (external input, constitution III)

A target is an attachment reference only if, after percent-decoding each segment: it is not empty
or longer than 1,024 characters; contains no NUL, control character or backslash; has no scheme
(`^[A-Za-z][A-Za-z0-9+.-]*:` — except the local token scheme `pbatt:`); does not start with `/`;
and the file name (last segment) is non-empty. `..` segments are allowed (the web editor writes
them; the server confines them) because the path is only ever sent to the server as a query
parameter and is never used to create a local path.

### Token operations (pure functions, Qt-free)

- `attachment_tokens(content)` → ids of all `pbatt:<id>` tokens in image syntax.
- `replace_token(content, id, remote_path)` → text with that token's target replaced by the
  percent-encoded server path (each segment `encodeURIComponent`-style).
- `strip_tokens(content, ids)` → text with the whole `![…](pbatt:<id>)` references removed, and one
  following newline removed if the reference stood alone on its line.

## Derived (not stored) structures

These are produced by the Qt-free parser and handed to the UI; none is persisted.

| Structure | Fields |
|-----------|--------|
| **Link** (inside a `Span`) | `url` (validated `http`/`https`), displayed text, `needs_confirmation` (spec FR-002). |
| **Table block** | `rows[]` of `cells[]` of spans; `align[]` per column (left/center/right/none); `weight[]` per column; first row is the header; `line_index` and `line_count` of the source lines. |
| **Attachment block** | reference target (validated path or token id), alt/display text, `kind`, plus — added by the bridge — the attachment row (state, size, error) if any. |
| **Emoji unit** | not a structure: the helper `utf8_cluster_end(text, pos)` returns where the unit starting at `pos` ends (code point plus variation selectors, skin-tone modifiers, combining marks, keycap; ZWJ-joined parts; regional-indicator pairs; tag sequences). |

Task-list toggling is unchanged and keeps using `line_index`; a table block spans several lines, so
`line_index` of later blocks is not shifted (every block keeps its own source line).

## Sync interaction (summary; algorithm in research R-3, R-4)

- `pending_notes()` is unchanged: a note with a pending attachment is `edited`, therefore pending in
  the list (spec FR-027).
- New store operations used by the sync engine: `pending_attachments(note_local_id)`,
  `record_uploaded(attachment_id, remote_path, size)` (one transaction: set state and path, replace the
  token in the note text and recompute title/excerpt, leave `modified` and `status` alone),
  `set_attachment_error(id, key)`, `discard_staged()` (binding of staged tokens happens inside `create_local`/`save_local`), `purge_unreferenced_downloads(note_local_id)` (after a pull changed the text),
  and, inside `resolve_conflict`, the re-binding/duplication rules of R-4 in the same transaction as
  the copy.
- Sync counts gain `attachments_uploaded` and `attachments_failed`; a failed attachment makes the
  outcome `Partial` and the status line says how many.
- Local data is never dropped for an attachment problem: a note's text is the source of truth and
  every attachment row can be rebuilt from it.

## Validation rules summary

- Add: the source must be a readable regular file ≤ 10 MiB; it is copied to `<id>.part`, size
  checked, renamed; any failure deletes the part file and leaves the note and text unchanged
  (spec FR-029, edge cases).
- Download: refused up front if `Content-Length` > 10 MiB; aborted if the received bytes exceed it;
  written to `.part` and renamed only when complete.
- Storage full: stop, set `storage_full`, keep everything saved so far (spec FR-026).
