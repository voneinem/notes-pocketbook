# Research: Explicit Note Title and Category Picker

Resolves what the plan's Technical Context left open. Facts were read from this repository (core,
bridge, QML, tests, feature 001–003 specs); nothing was probed on a server or the device. Items
marked **[VERIFY]** need a check during implementation and have a stated fallback.

## R-1. Where the explicit title lives

- **Findings**: `notes.title TEXT NOT NULL` exists since schema 1 and is already what the list,
  search, sort and reader show. Today it is *written* from the text in four places: `create_local`,
  `update_locked` (every edit), the three attachment-upload statements in
  `note_store_attachments.cpp` (they rewrite a token in the text and recompute title and preview)
  and the conflict copy; and from the server by `title_for` on pull and `confirm_push`.
- **Decision**: reuse the column; stop deriving it. `create_local(title, content, …)` and
  `save_local(id, title, content, …)` store the validated, trimmed title; `set_category`,
  `set_favorite`, `toggle_task` and the attachment rewrite keep the stored title and recompute only
  the preview. Pull and push confirmation keep using `title_for` (the server's title), so a
  synced note shows what the server reports (FR-007).
- **Alternatives considered**: a new `title_explicit` column to tell guessed from chosen titles
  (needs a migration, branches in push, pull and conflict code, and the reader gains nothing —
  rejected, see R-6); keeping `derive_title` as a fallback when the title is empty (it is the
  guess the feature removes, and FR-004 makes an empty title unsaveable).

## R-2. Title rules (FR-004, FR-005)

- **Decision**: `validate_title(title)` mirrors `validate_category` so the two behave alike:
  valid UTF-8; no control characters (U+0000–U+001F, U+007F — this includes line breaks and tabs);
  at most 255 code points; **after trimming** ASCII space (and tab/CR/LF, which can only occur at
  the ends of pasted text, before the control-character rule would reject them) it must be
  non-empty. Error keys: `title_empty`, `title_too_long`, `title_control_chars`, `title_invalid`.
  The store trims, then validates, then stores the trimmed value; the editor shows the same
  messages as for categories.
- **Why not reject trailing spaces like categories do**: a title is typed into a field the reader
  can't see the end of; trimming is friendlier (spec FR-005 says "trim"). Categories keep their
  rule (reject) because `validate_category` and its tests already exist and the picker trims typed
  names itself (R-5).
- **Server-restricted characters** (`/ \ : * ? " < > |` and similar in a file name): not rejected.
  The server stores a name it can and reports it back; the app shows the reported title after the
  next sync (spec edge case). See R-3.
- **Alternatives considered**: forbidding file-name characters in the app (guesses at the server's
  rules, and the Notes server may differ by version); a length limit below 255 (the category rule
  already uses 255).

## R-3. Sending the title (FR-008) — **[VERIFY]** server behavior

- **Findings**: `draft_json` sends `content`, `category`, `favorite`, `modified`; the contract in
  feature 001 says "title is never sent". The Notes API v1 note object has a `title` field; on
  write, an absent title makes the server derive it from the first line of the content (the
  behavior this feature must stop relying on), a present title is used for the note's file name.
- **Decision**: add `"title"` to every create and update body, including updates that change only
  the category or favorite (otherwise a text-only change would let the server re-derive and rename
  the file). The answer's `title` continues to be stored via `title_for`.
- **[VERIFY] real server**: (a) an explicit title is honored on `POST` and `PUT`; (b) a duplicate
  title in the same category is made unique by the server (for example with a numeric suffix) and
  reported back; (c) forbidden characters are replaced and reported back. Check once with a
  disposable note in quickstart §3. **Fallback** if (a) fails (server ignores the title): the app
  keeps working — it shows the server's reported title after each push, as the spec's last
  assumption already allows — and the feature is limited to local titles; record the finding in the
  quickstart and open a follow-up.
- **Alternatives considered**: sending the title only when it changed (saves bytes but needs
  "title changed since last push" bookkeeping and still risks re-derivation); `PATCH` (not part of
  the API the app uses).

## R-4. Conflicts must see the title (constitution VI)

- **Findings**: `resolve_conflict` adopts the server version silently when "content, category and
  favorite are identical". With titles editable, a title-only edit on one side and an unrelated
  rename on the other would take that shortcut and lose the reader's title. The conflict copy is
  named by `with_title_suffix(content, suffix)`, which edits the first line of the *text*.
- **Decision**: the shortcut also requires equal titles (`local.title == title_for(server)`). A
  copy keeps the local text byte for byte and gets `title = local.title + suffix` (suffix and
  numbering unchanged: " (conflict YYYY-MM-DD)", " #n"); uniqueness is still checked against
  titles. `with_title_suffix` is removed (nothing else uses it). The copy is made by an internal
  write that does not apply the 255-character limit (a conflict copy must never be refused; the
  suffix adds at most 30 characters), so the original title is kept whole.
- **Legacy case**: a pre-feature unsynced edit whose server title differs from its stored title
  (the server file was renamed meanwhile) now yields a conflict copy instead of a silent adopt.
  A spurious copy is harmless (both versions are kept); a silent adopt would not be.
- **Alternatives considered**: ignoring the title in the identity test (loses a title edit);
  merging titles by modified time (a guess, forbidden by the constitution).

## R-5. The category picker (FR-011 – FR-020)

- **Findings**: `NoteStore::categories()` already returns the non-empty categories of all
  non-deleted notes (including unsynced ones) with counts, sorted case-insensitively. The
  note-list view picker already uses `ChoiceDialog` (a paged modal list of `{label, value}` with
  a ✓ on `current`, page buttons, Cancel) and `NotesBridge::categories()`.
- **Decision**: reuse `ChoiceDialog` unchanged. Values are `"none"`, `"new"` and `"c:" + name`, so
  a category called "none" cannot be confused with the action. Order: "No category", "New
  category…", then the categories (FR-019: the two actions stay on the first page). The current
  choice is always listed and marked, even if it exists only in this unsaved editor session
  (FR-017 lets the other notes' pickers omit it). New bridge call `categoryChoices(current)` returns
  the list; QML adds no logic.
- **New-category entry**: a new `TextEntryDialog` (title, placeholder, error line, Confirm/Cancel;
  full-screen modal like the other dialogs; the on-screen keyboard shows). The bridge call
  `checkCategory(name)` trims the name, runs `validate_category` and returns
  `{ok, name, error}`; an exact match with an existing category selects it (FR-016) without special
  code because the value is just the string. Matching is exact and case-sensitive (spec edge case).
- **Where the choice lives**: in the editor's state (`category` property) until Save, like the
  other fields; `dirty` includes it (already so). No store change is needed for new categories:
  a category exists as long as a note carries it (spec FR-018, already true).
- **Alternatives considered**: a type-ahead filter box over the existing categories (more keyboard
  use on e-ink — the thing this feature removes); a separate "manage categories" screen (rename and
  delete are out of scope); adding the sentinel entries inside `ChoiceDialog` (couples a generic
  component to one use).

## R-6. Notes written before this feature (FR-021, edge cases)

- **Findings**: unsynced notes (`status='edited'`) hold `derive_title(content)` — the raw first line,
  including a heading marker (`## Plan`); the list showed it without the marker
  (`display_title`). Synced notes hold the server's title. All previews were cut at the old rule
  ("skip the first non-empty line").
- **Decision**: schema 3 is a data-only migration inside the existing pre-migration backup and
  transaction: for rows with `status='edited'`, set `title = legacy_title(title)` (the
  rule `display_title` applied, moved into the core, plus the title limits: an old title was the whole
  first line, possibly over 255 characters or holding a tab — the push guard would otherwise refuse
  such a note forever), and recompute `excerpt` for all rows with the
  new rule (R-7). Nothing else changes: no note is renamed, recategorized or marked edited by
  opening this version (FR-021). `display_title` stops stripping markers so a title shows exactly as
  entered (FR-007); a title that is empty (only possible for a legacy row with neither text nor
  server title) shows "(untitled)".
- **Alternatives considered**: no migration and sending `## Plan` as the title (puts the marker in
  the server's file name — a visible guess); a `title_explicit` flag (R-1); stripping at push time
  only (the editor would show the raw marker).

## R-7. Preview line (FR-023)

- **Findings**: `make_excerpt` always skips the first non-empty line because that line *was* the
  title. With an explicit title this would silently drop the first line of new notes from the list
  preview.
- **Decision**: `make_excerpt(text, title, max_chars)` skips the first non-empty line only if, after
  `strip_heading_marker` and trimming, it equals `strip_heading_marker(title)` (byte-equal otherwise; the
  title of a synced row can itself start with `#`). Otherwise the excerpt
  starts at the beginning of the text. Callers pass the stored title. Existing synced notes (title
  = first line) behave as before.
- **Alternatives considered**: never skipping (duplicates the title for every old note); always
  skipping (loses text).

## R-8. Editor behavior

- **Decision**: layout order Title field → Category button → text. Title is a one-line `TextField`
  (Return moves focus to the text, never inserts a line break), `inputMethodHints:
  Qt.ImhNoPredictiveText` like the others. A new note focuses the Title field; an existing one
  keeps the cursor at the end of the text as today. `dirty` adds the title. Save with an invalid
  title shows the message and keeps the editor (FR-004); the core is the only judge — the editor
  does not duplicate the rule.
- **Reader/list**: show the stored title as is (`display_title` no longer rewrites it).
- **Alternatives considered**: a modal "rename" dialog like the Android app (an extra tap per edit
  and the title would be invisible while writing).

## R-9. Test plan (written first, constitution I)

- **Unit**: `validate_title` table (empty, spaces, 255/256 code points incl. multi-byte, tab/CR/LF,
  control, invalid UTF-8); store edits (title stored and trimmed, unchanged by text/category/favorite/
  toggle/attachment-rewrite edits, rejected title keeps the previous note); `make_excerpt` with and
  without a repeated title (heading marker, case difference = not repeated); `strip_heading_marker`;
  conflict identity with a title-only difference, copy naming from the title with the text
  untouched, numbering, atomicity (extends `test_conflict_atomic`); migration v2 → v3 using an
  anonymized version-2 database (edited rows stripped, synced rows untouched, previews recomputed,
  backup removed, rollback on failure); `categories()` includes unsynced categories.
- **Contract**: `POST`/`PUT` bodies carry `title` with correct JSON escaping, including emoji,
  quotes and backslashes; an answer with an adjusted title is stored as reported.
- **Integration**: push of a title-only edit; title-only edit on both sides → conflict copy, both
  kept; offline title edit pushed on the next sync; pull of a changed server title onto a note
  without local changes; a legacy pending note pushes its stripped title.
- **Bridge**: `loadForEdit` returns the title; `saveNote` with a bad title returns `-1` and emits
  the error text; `categoryChoices` content/order/marked current; `checkCategory` trims, rejects
  empty/too long/control, accepts `/`; `display_title` no longer strips markers.
- **Manual (device)**: quickstart §3 (picker paging, keyboard, e-ink refresh, server [VERIFY]).
