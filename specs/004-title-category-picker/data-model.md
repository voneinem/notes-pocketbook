# Data Model: Explicit Note Title and Category Picker

No new table or column. The `notes` table of feature 001 (schema in `core/src/note_store.cpp`)
keeps its shape; what changes is who writes `title` and the rules it must satisfy. Decisions:
[research.md](research.md).

## Note (changed fields)

| Field | Before | After |
|-------|--------|-------|
| `title` | `derive_title(content)` for local edits, the server's title for notes in sync | the title the reader entered (validated, trimmed) for local edits; the server's reported title for notes in sync (as before) |
| `excerpt` | text after the first non-empty line | text from the start, minus a first line that only repeats `title` (heading markers ignored on both sides, FR-023) |
| `category` | free text typed in the editor | unchanged value space; chosen from the picker or typed through "New category…" |

Everything else (`content`, `favorite`, `modified`, `etag`, `status`, `remote_id`) is unchanged.

### Title rules (FR-004, FR-005, FR-010)

- Trimmed of leading/trailing ASCII whitespace before validation and storage.
- Non-empty after trimming; at most 255 code points; valid UTF-8; no control characters
  (U+0000–U+001F, U+007F), hence no line breaks.
- Not unique: two notes may have the same title (the server may disambiguate its file name; the
  app then shows the reported title).
- Never derived from `content`. Exception: a *synced* row whose server gives no title and whose
  text is empty is shown as "(untitled)" (legacy only; cannot be created by this version).

### Who may change `title`

| Operation | Title |
|-----------|-------|
| `create_local`, `save_local` | set from the argument |
| `set_category`, `set_favorite`, `toggle_task` | kept |
| attachment token rewrite after upload (`record_uploaded` and the two paths in `note_store_attachments.cpp`) | kept (only `excerpt` is recomputed) |
| `apply_remote` on a `void` row, `confirm_push` of exactly what was sent | set to `title_for(server)` |
| `apply_remote` on an `edited`/`deleted` row | kept (local changes win until pushed) |
| `resolve_conflict` | original: server's title; copy: `local.title + suffix` |

### Category (derived, unchanged)

A category is the set of notes carrying a name. The picker's list is
`categories()` ∪ {current choice of the open editor}; sorted case-insensitively (existing order),
duplicates impossible (exact string match). A category with no remaining note disappears (FR-018).
Rules for a name: `validate_category` after trimming (≤ 255 code points, no control characters,
valid UTF-8; `/` allowed and flat).

## Conflict identity (changed)

A pushed `edited` note whose server copy changed (HTTP 412) is "identical" — and silently adopts
the server version — only if **title**, content, category and favorite are all equal
(`local.title == title_for(server)`). Otherwise a conflict copy is kept first, then the original is
replaced; the copy has the local text unchanged and `title = local.title + " (conflict YYYY-MM-DD)"`
(` #n` for n ≥ 2).

## Push snapshot (changed)

`PushSnapshot` carries `title`; `confirm_push` clears `edited` only if title, content, category and
favorite still equal what was sent.

## Schema change: version 2 → 3 (data only)

Runs once through the existing migration path (backup `<db>.bak`, one transaction, integrity check,
backup removed on success):

1. For rows with `status = 'edited'`: `title = legacy_title(title)` — the marker the list never showed
   (`## Plan` → `Plan`) must not become an explicit title, and a long first line (an old title was
   the whole line) is made to satisfy the title rules: control characters become spaces, the result
   is trimmed and cut after 255 code points ("Untitled" if nothing is left), so the first push is
   never refused for a title the user did not choose.
2. `UPDATE notes SET excerpt = make_excerpt(content, title)` for every row (new preview rule).

New databases start at version 3 with the unchanged `CREATE TABLE` statements. A version-3
database opened by an older build is refused by the existing "written by a newer version" rule.
No row's `status`, `modified`, `etag`, `remote_id`, `category` or `content` is touched (FR-021).
