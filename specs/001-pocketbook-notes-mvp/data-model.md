# Data Model: PocketBook Notes MVP

SQLite database `pbnotes.db` (device: `/mnt/ext1/system/pbnotes/`, host: `./pbnotes-data/`).
Pragmas on open: `journal_mode=TRUNCATE`, `synchronous=FULL`, `foreign_keys=ON`,
`secure_delete=ON`, `busy_timeout=5000`. Schema version in `PRAGMA user_version`. All writes happen in short
transactions. Before any schema migration the file is copied to `pbnotes.db.bak`; the backup is deleted as soon as the migration and an integrity check have succeeded, because it would otherwise hold a second plaintext copy of the account password.

## Entities

### Account (exactly one row, `id = 1`, absent before setup)

| Field | Type | Rules |
|-------|------|-------|
| server_url | text | `https://` only, normalized without trailing slash; sub-path allowed |
| username | text | non-empty |
| password | text | Nextcloud app password; **stored unencrypted** (see research R-13 #4); never logged, never shown after entry |
| collection_etag | text, nullable | ETag of the last notes list response |
| collection_last_modified | integer | epoch seconds from the list response (`Last-Modified`), 0 if unknown; used as `pruneBefore` |
| api_versions | text | server-advertised versions, e.g. `1.0,1.3` |
| last_sync_at | integer, nullable | epoch seconds of the last finished attempt |
| last_sync_outcome | text | `never`, `success`, `partial`, `failed`, `offline`, `maintenance` |
| last_sync_message | text | short human-readable detail (no note content) |
| last_sync_counts | text | JSON `{pushed, pulled, deleted, conflicts}` |

### Note

| Field | Type | Rules |
|-------|------|-------|
| local_id | integer PK | autoincrement, never reused |
| remote_id | integer, nullable, unique | null = never uploaded |
| status | text | `void`, `edited`, `deleted` (see state machine) |
| title | text | display title. For a note in sync (`void`): the title the server reports (its file name, as the web and Android apps show it), or the first non-empty line of `content` when the server reports none. For a note with local changes: the first non-empty line of `content`, recomputed on every save. Set on every pull and when a push is confirmed. A note with neither text nor title is shown as "(empty note)". `settings.titles_from_server` marks that the stored titles come from a complete pull; older databases get one full pull to refresh them |
| content | text | full note text (UTF-8), includes the title line |
| category | text | empty string = uncategorized; `/` is allowed and shown as typed (flat list); validated, see below |
| favorite | integer | 0/1 |
| modified | integer | epoch seconds; set locally on save, replaced by the server value after push/pull |
| etag | text, nullable | per-note ETag from the server; used as `If-Match` |
| excerpt | text | derived (first ~120 chars after the title), never sent |

Indices: `(status)`, `(category)`, `(favorite)`, `(modified)`, `(remote_id)`.

Validation rules (from the spec and API):
- A new note whose content is empty or whitespace-only is not stored (FR-020).
- `category` max 255 characters, no leading/trailing space, no control characters (U+0000-U+001F,
  U+007F); `/` is allowed. Otherwise the editor shows "Invalid category" and keeps the previous
  value.
- Title is never edited separately (FR-019).
- Search matches case-insensitively on `title` and `content` (substring), implemented in the store
  with `LIKE` using an escape character, or with an application-side fold for non-ASCII text
  (decide when writing the store tests; SQLite `LIKE` is ASCII-only case-insensitive).

### Setting (key/value table `settings`)

| Key | Values | Default |
|-----|--------|---------|
| font_size | `small`, `medium`, `large` | `medium` |
| sort_order | `modified` (newest first), `title` | `modified` |
| view | `all`, `favorites`, `uncategorized`, `category:<name>` | `all` |
| rotation | `auto`, `portrait`, `landscape` | `auto` |

### Derived / non-persistent

- **Category list**: computed from notes (`SELECT category, COUNT(*) ... WHERE status != 'deleted'`),
  empty category excluded; a category exists while at least one note uses it.
- **Conflict Copy**: an ordinary Note row (status `edited`, no remote id) whose title is the
  original title plus a conflict marker with the date, e.g. `<original title> (conflict 2026-10-10)`.
  If that title already exists among local notes, a counter is appended (`... (conflict 2026-10-10 #2)`).
  Because the title is the first line, the marker is written into the first line of the copy's
  content as well.
- **Pending Change**: any Note with status `edited` or `deleted`.
- **Sync Status**: the `last_sync_*` fields plus an in-memory `running` flag.

## State machine (Note.status)

```text
            save (new)                       push 200/201
(none) ───────────────────► edited(no remote) ───────────────► void
                                  ▲                              │ ▲
              save / favorite /   │                              │ │ pull update
              category / checkbox │                    save etc. │ │ (only when void)
                                  │                              ▼ │
                              edited(remote) ◄──────────────── void
                                  │   │
              push 200            │   │ push 412 → conflict
              ───────────────────►│   │   original := server version (void)
              void                │   │   new note := local text, edited, no remote_id
                                  │   │ push 404 → POST (re-create), remote_id replaced
                                  ▼
      delete (with remote id) → deleted ── DELETE ok / 404 ──► row removed
      delete (no remote id)   → row removed immediately
      deleted + server changed (recheck 200) → server version restored as void, user informed
```

Rules:
- Pull never changes a note whose status is not `void`.
- A pull deletes a local `void` note only when its remote id is missing from the server list.
- A local change is considered confirmed only when the server response is stored in the same
  transaction that sets status `void` (FR-036).
- If the user changes a note while its push is in flight, the push result is stored but the note
  stays `edited` when content, category or favorite differ from what was sent.
- A conflict is resolved in ONE transaction: the conflict copy is inserted first (and verified to
  equal the local text apart from the conflict marker on its title line) and only then is the original replaced by the server version.
  A failure rolls back both; the local text is never held in zero rows. If the local and server
  versions are identical in content, category and favorite, no copy is created: the original just
  becomes `void` with the server's etag and modified time, and no conflict is counted.

## Relationships

- One Account : many Notes (the table has no account column because only one account exists;
  adding multi-account later would be a schema migration).
- Notes reference no other entity; categories are values on notes.
- A Conflict Copy has no database link to its original; it is traceable only by title.

## Volume assumptions

Up to ~1,000 notes, a few KB each (about 1-10 MB total); full-table scans for search are acceptable
(SC-008) and verified by a host benchmark test with 1,000 generated notes.
