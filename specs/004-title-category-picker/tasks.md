---

description: "Task list for the explicit note title and the category picker"
---

# Tasks: Explicit Note Title and Category Picker

**Input**: Design documents from `/specs/004-title-category-picker/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: REQUIRED. The constitution makes Test-First non-negotiable (Principle I): every test task
below MUST be written and observed FAILING before the implementation task(s) it names (a test that
does not compile against the old signature counts as failing). Tests for `notes_core` run on the host
without Qt, the SDK or a device (Principle V). Unit, contract and integration tests under
`tests/unit/`, `tests/contract/`, `tests/integration/` are picked up by the glob in
`tests/CMakeLists.txt`; bridge tests live in `tests/bridge/` and are already listed there (this
feature adds no new bridge test executable).

**Organization**: Tasks are grouped by user story. Paths are relative to the repository root. This
feature builds on the finished MVP and features 002–003; all existing tests must stay green.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US3 as in spec.md
- **[MANUAL]**: needs a person or the reader; record the result in the named file. An implementing
  agent skips these and reports them as open

## Conventions that apply to every task

- C++17; namespace `pbnotes`; `notes_core` MUST NOT include Qt or `inkview.h` headers.
- TLS verification, SDK paths and logging rules are those of the constitution and are enforced by
  `scripts/check-constitution.sh`: SDK or toolchain paths MUST NOT appear in any committed file, and
  logs carry event names and counts only — **no title, category name or note text in any log line**.
- The title is never derived from the text and never guessed: no code path may call `derive_title`
  to fill a title that is stored or sent. (`derive_title` stays only for `title_for`'s fallback when a
  server note has no title, and in tests.)
- Title rules, quoted from `data-model.md` (apply in code, not by convention): "Trimmed of
  leading/trailing ASCII whitespace before validation and storage. Non-empty after trimming; at most
  255 code points; valid UTF-8; no control characters (U+0000–U+001F, U+007F)." Error keys:
  `title_empty`, `title_too_long`, `title_control_chars`, `title_invalid`.
- Category rules are the existing `validate_category` ones: "at most 255 characters, no leading or
  trailing space, no control characters (U+0000-U+001F, U+007F), valid UTF-8; "/" is allowed and the
  empty string means uncategorized". The picker trims typed names first (`checkCategory`).
- UI strings go through `qsTr`; titles and category names are shown as stored.
- Interfaces are those of `contracts/core-library.md`, `contracts/notes-api-title.md` and
  `contracts/qml-bridge.md`; if a task finds a contract wrong, fix the contract in the same commit.

---

## Phase 1: Setup

- [X] T001 Build the host target and run the whole suite on the untouched branch
  (`cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-host && ctest
  --test-dir build-host --output-on-failure`). Every test must pass; if one does not, stop and report
  it — later failures must be attributable to this feature.

---

## Phase 2: Foundational (blocking prerequisites)

**Purpose**: The pure helpers every story uses: the heading-marker rule, the title-aware preview, and
title validation. No behavior of the app changes yet.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

### Tests first (observe failing)

- [X] T002 [P] Extend `tests/unit/test_title.cpp` for `contracts/core-library.md` (title.h):
  `strip_heading_marker`: `"## Plan"` → `"Plan"`, `"# x"` → `"x"`, `"###### six"` → `"six"`, seven `#`
  followed by space unchanged, `"#hashtag"` unchanged, `"###"` unchanged, `"  ## Plan  "` → `"Plan"`,
  tab after the marker counts as whitespace, multi-byte text untouched.
  `make_excerpt(text, title)`: first line equal to the title is dropped (also when it is `"## Title"`
  and the title is `"Title"`); first line different from the title is kept (preview starts at the
  start of the text); equality is byte-exact (`"title"` vs `"Title"` keeps the line); empty title
  never drops anything; a text that is only the title gives an empty preview; max_chars and the
  never-cut-a-display-unit rule still hold. Convert the existing `make_excerpt` calls to the new
  signature passing `derive_title(text)` so their expectations stay as they are.
- [X] T003 [P] Write `tests/unit/test_title_validation.cpp` for `validate_title` and `trim_title`
  (`contracts/core-library.md`): `""`, `"   "`, `"\t\r\n"` → `title_empty`; `"Groceries"` and
  `"  Groceries  "` → valid and trimmed to `"Groceries"`; exactly 255 code points valid (ASCII and a
  string of 255 two-byte letters), 256 → `title_too_long` (count code points, not bytes; a title of
  255 emoji is valid); `"a\nb"`, `"a\tb"` (inner tab), `"a\x01b"`, `"a\x7f"` → `title_control_chars`;
  trailing `"\n"` is trimmed and therefore valid; invalid UTF-8 (`"\xC3"`, `"\xC0\xAF"`, overlong,
  surrogate) → `title_invalid`; `/ \ : * ? " < > |` inside a title → valid; precedence when several
  rules fail is invalid UTF-8, then control characters, then length.

### Implementation

- [X] T004 Implement in `core/src/title.cpp` / `core/include/pbnotes/title.h`: `strip_heading_marker`
  and the new `make_excerpt(text, title, max_chars = 120)` exactly as in `contracts/core-library.md`.
  Move the marker logic out of `display_title` in `bridge/src/notes_bridge.cpp` (the bridge keeps
  its own copy until T020). Keep `derive_title`, `fold_case`, `conflict_suffix`; keep
  `with_title_suffix` until T019. Update every existing `make_excerpt(content)` call in
  `core/src/note_store.cpp` (insert/update on pull, create, update, conflict, restore) and
  `core/src/note_store_attachments.cpp` (the three `up.bind_text(3, make_excerpt(updated))` places) to
  pass the title being written in that statement, or for the attachment rewrites the row's stored
  title read in the same transaction — behavior unchanged for now. Make T002 pass.
- [X] T005 Add to `core/include/pbnotes/note_store.h` / `core/src/note_store.cpp`: `trim_title`,
  `validate_title` (per T003 and `data-model.md`, mirroring `validate_category`'s UTF-8 walk), and
  `EditError::InvalidTitle`. No signature of an existing function changes yet. Make T003 pass and
  run the whole suite (still green).

**Checkpoint**: helpers exist and are tested; the app behaves as before.

---

## Phase 3: User Story 1 - Give a note an explicit title (Priority: P1) 🎯 MVP

**Goal**: A separate Title field in the editor; the stored title is what the list, reader and server
show; editing the text never changes it; unsynced and conflict paths never lose a title.

**Independent Test**: Create a note titled "Groceries" whose text starts with another line; save;
edit the first line; save; the title is still "Groceries". Change only the title; the text is
unchanged. A new note with text but no title is refused with a message and keeps its text.
(quickstart §3 rows 1–4 and 14–17; US1 acceptance scenarios 1–8.)

### Tests first (observe failing)

- [X] T006 [P] [US1] Extend `tests/unit/test_store_edit.cpp` using the signatures
  `create_local(title, content, category, favorite, now)` and `save_local(id, title, content,
  category, favorite, now)`: the title is stored trimmed; `"  Groceries  "` is stored as `"Groceries"`;
  a text-only `save_local` with the same title keeps the title even when the first line changes; a
  title-only change marks the note `edited` and leaves content byte-equal; `set_category`,
  `set_favorite` and `toggle_task` keep the title; an unchanged save changes nothing (modified time
  stays); each invalid title (`title_empty`, `title_too_long`, `title_control_chars`, `title_invalid`)
  returns `EditError::InvalidTitle` with that `error_key` and leaves the stored note untouched; check
  order is title, then content, then category; new note with empty title and whitespace-only text →
  `content_empty` and nothing stored; new note with text and empty title → `title_empty` and nothing
  stored; the excerpt follows FR-023 (a first line equal to the title is dropped, any other first
  line stays).
- [X] T007 [P] [US1] Extend `tests/unit/test_store_attachments.cpp` (and
  `tests/unit/test_store_attachments_staging.cpp` where it calls the edit functions): after
  `record_uploaded` rewrites a token in the text, the note's title is unchanged (also when the first
  text line is the token line), `modified`/`status`/ETag are unchanged as before, and the excerpt is
  recomputed with the stored title.
- [X] T008 [P] [US1] Extend `tests/unit/test_store_pull.cpp`: pull onto a `void` note stores
  `title_for(server)` (the server's title, not the first line); a server title different from the
  first line shows the server's title; an `edited` note keeps its local title when the server title
  changes; the excerpt of pulled notes follows FR-023 with the stored title.
- [X] T009 [P] [US1] Write `tests/unit/test_store_migration_v3.cpp` in the style of
  `tests/unit/test_store_migration_v2.cpp` (build a version-2 database by hand with anonymized rows):
  `kSchemaVersion == 3`; opening a version-2 database yields version 3; an `edited` row titled
  `"## Plan"` becomes `"Plan"`, a `void` row's title is untouched even if it starts with `#`;
  excerpts of all rows are recomputed with the FR-023 rule (a row whose first line is not its title
  keeps that line in the preview); status, modified, etag, remote_id, category, favorite and content
  of every row are unchanged; no `.bak` remains; with `after_backup_hook` returning false the open
  fails, the database stays at version 2 and the backup is kept; a 5,000-row database migrates in
  under 5 seconds; opening a version-4 database is refused ("written by a newer version"). Update the
  version expectations in `tests/unit/test_store_migration_v2.cpp` (`the_schema_version_is_two`,
  `schema_version(), 2` at lines 94/103/161 now describe the v1→v2 step: assert `kSchemaVersion`
  where they mean "current", keep literal 2 only where a v2 database is built by hand).
- [X] T010 [P] [US1] Extend `tests/contract/test_api_write.cpp` and add the anonymized fixture
  `tests/fixtures/note_created_adjusted_title.unverified.json` (answer whose `title` differs from the
  sent one): the `POST` and `PUT` bodies contain `"title"` with the draft's title, also for an update
  that changes only category or favorite; JSON escaping of quotes, backslash, emoji and non-ASCII in
  the title; the body has no other new key; an answer with an adjusted title is returned as the note's
  `server_title` unchanged. Run `scripts/check-fixtures.sh tests/fixtures` for the new file.
- [X] T011 [P] [US1] Rewrite `tests/unit/test_conflict_title.cpp` and extend
  `tests/unit/test_conflict_atomic.cpp` for the title-based conflict copy
  (`contracts/core-library.md`): `conflict_suffix` cases stay; `resolve_conflict` makes a copy with
  `title == local.title + " (conflict YYYY-MM-DD)"` and `content == local.content` byte for byte
  (including a first line that is not the title, CRLF content and a content that is only the
  title line); numbering ` #2`, ` #3` while a note with that exact title exists; the copy's title is
  not shortened or validated against 255 even when the original title has 255 code points; the
  "identical, no copy" shortcut applies only when title, content, category and favorite all match
  (server title differing → a copy is made and neither version is lost); failure injection
  (`test_fail_nth_write`) at every write leaves the local text in at least one row and no half
  state; remove the `with_title_suffix` cases.
- [X] T012 [P] [US1] Extend `tests/integration/test_sync_push.cpp`: a created note's title is sent on
  `POST`; an update sends the title even when only category/favorite changed; a title-only edit is
  pushed and the note becomes `void`; `confirm_push` leaves a note `edited` if the title (not only
  text) changed while the request was in flight; an offline title edit (transport failing, then
  succeeding) reaches the server on the next sync and nothing is lost; a server answer with an
  adjusted title makes the list/store show the adjusted title; a legacy `edited` note stored with
  `"Plan"` (after migration) is pushed with `"Plan"`; a stored title failing `validate_title` is not
  pushed, counts as a failed note and its text is untouched. Extend
  `tests/integration/test_sync_conflict.cpp`: a title-only edit on the device and an unrelated change
  on the server (412) → two notes after sync, the server's version and the ` (conflict …)` copy with
  the device's title, both uploaded; a title changed on the server of a note without local changes
  appears after the next sync (US1-8).
- [X] T013 [P] [US1] Extend `tests/bridge/test_notes_bridge.cpp`: `loadForEdit` returns `title` as
  stored (including a leading `#`); `saveNote(id, title, text, category, favorite)` stores the title;
  a bad title returns `-1`, emits `errorOccurred` with the texts `title_empty` ("The note needs a
  title."), `title_too_long` ("The title is too long (at most 255 characters)."),
  `title_control_chars`/`title_invalid` ("The title contains characters that are not allowed."), and
  stores nothing; `listNotes()[].title` and `openNote().title` return the stored title as is (a title
  `"## Plan"` is shown as `"## Plan"`); an empty stored title (legacy) shows `"(untitled)"`.

### Implementation

- [X] T014 [US1] Change `core/include/pbnotes/note_store.h` and `core/src/note_store.cpp`:
  `create_local(title, content, category, favorite, now)` and `save_local(local_id, title, content,
  category, favorite, now)`; `update_locked` takes `std::optional<std::string> title` (nullopt for
  `set_category`, `set_favorite`, `toggle_task`, which keep the stored title); trim then validate
  (check order title → content → category; first failure returned, nothing stored); store the trimmed
  title and recompute only `excerpt` with it; `Note::title` comment in `core/include/pbnotes/types.h`
  becomes "stored explicit title; server's title for notes in sync"; add `title` to
  `PushSnapshot` and make `confirm_push` compare title, content, category and favorite. Remove the
  `derive_title` calls from every write path (`create_local`, `update_locked`, conflict copy is T019).
  Update every existing caller in the tests mechanically so the whole tree compiles
  (`tests/unit/test_store_search.cpp`, `test_store_attachments_staging.cpp`, `test_account_store.cpp`,
  `test_category_validation.cpp`, `test_emoji_roundtrip.cpp`, `test_conflict_atomic.cpp`,
  `test_store_failure.cpp`, `test_store_views.cpp`, `test_attachment_service_add.cpp`,
  `tests/bridge/test_notes_bridge.cpp`, `tests/integration/test_edge_cases.cpp`,
  `test_sync_delete_edit.cpp`, `test_sync_conflict.cpp`, `test_sync_robustness.cpp`,
  `test_sync_push.cpp`, `test_toggle_sync.cpp`, `test_perf_sync.cpp`,
  `test_sync_attachments_push.cpp`): pass `derive_title(content)` where the test does not care about
  the title, so their expectations stay valid. Make T006 pass; run the unit tests.
- [X] T015 [US1] In `core/src/note_store_attachments.cpp` change the three statements that rewrite a
  token (`up.bind_text(2, derive_title(updated))` at the former lines 368, 416, 478): they no longer
  write `title`, only `content` and `excerpt` (computed with the row's stored title). Make T007 pass.
- [X] T016 [US1] Make T008 pass (adjust `title_for` / `apply_remote` only if the test finds a gap; the
  comments in `core/src/note_store.cpp` around `title_for` and `core/include/pbnotes/types.h`
  `ApiNote::server_title` are updated to say the server's title is what a synced note shows, with the
  first line only as the fallback for a server note without a title).
- [X] T017 [US1] Schema 3: set `kSchemaVersion = 3` in `core/include/pbnotes/note_store.h`; in
  `NoteStore::open` (`core/src/note_store.cpp`, migration block) run, inside the existing
  transaction and after `kSchemaSql`, for databases with `version >= 1`: set
  `title = strip_heading_marker(title)` for rows with `status='edited'`, and recompute `excerpt` with
  `make_excerpt(content, title)` for every row (prepared statements, no string-built SQL). The
  existing backup, integrity check and rollback path stay. Make T009 pass.
- [X] T018 [US1] API and sync: add `std::string title` to `NoteDraft` in
  `core/include/pbnotes/api_client.h`; `draft_json` in `core/src/api_client.cpp` writes
  `"title"` first, quoted through the existing JSON quoter; update the comment above `NoteDraft`
  ("The title is never sent" is no longer true). In `core/src/sync_engine.cpp` `draft_of`
  and `snapshot_of` carry `note.title`; before pushing, a note whose stored title fails
  `validate_title` is skipped as a failed note (counted like other failed notes, log only the event
  name and a count) and its local text is left alone. Make T010 and T012 (push part) pass.
- [X] T019 [US1] Conflicts: in `NoteStore::resolve_conflict` (`core/src/note_store.cpp`) the identical
  shortcut also requires `local.title == title_for(server)`; the copy is inserted with
  `title = local.title + suffix` and `content = local.content` (check the re-read of the copy compares
  title and content), numbering by testing `SELECT COUNT(*) FROM notes WHERE title=?` against
  `local.title + suffix`; remove `with_title_suffix` from `core/include/pbnotes/title.h` and
  `core/src/title.cpp`. Make T011 and the conflict part of T012 pass.
- [X] T020 [US1] Bridge (`bridge/include/notes_bridge.h`, `bridge/src/notes_bridge.cpp`):
  `loadForEdit` adds `title`; `saveNote(localId, title, text, category, favorite)` passes the title to
  the store; `display_title` returns the stored title as is and `"(untitled)"` if empty (drop the
  heading-marker removal and the `"(empty note)"` text); add the four title error texts to the error
  table next to the `category_*` texts; update the header comments. Make T013 pass.
- [X] T021 [US1] Editor page `qml/pages/EditorPage.qml`: add `startTitle` and a Title row (label +
  one-line `TextField`, `font.pixelSize: 30`, `inputMethodHints: Qt.ImhNoPredictiveText`, same black
  border style as the category field) above the Category row; `Keys.onReturnPressed` and
  `Keys.onEnterPressed` move focus to the text area and never insert a line break; `dirty` includes
  `titleField.text !== startTitle`; `Component.onCompleted` loads the title, a new note focuses the
  Title field, an existing note keeps today's focus and caret at the end of the text; `save()` calls
  `bridge.saveNote(noteId, titleField.text, area.text, categoryField.text, favorite)` and on `-1`
  stays on the page showing `message` and keeping all typed text; update the header comment (the
  first line is no longer the title). The category field stays a text field until US2.
- [X] T022 [US1] Run the host build and `ctest` in full, plus the app `--self-test`
  (`app_self_test`); all green. Run `scripts/check-constitution.sh .` (no title in logs, no SDK
  paths).
- [x] T023 [MANUAL] [US1] Build and install the package on the reader (INSTALL.md) and walk through
  quickstart §3 rows 1–4 and 14–17 (including the **[VERIFY]** server behavior of row 16). Record
  results and any deviation in `specs/004-title-category-picker/checklists/device-results.md`.

**Checkpoint**: titles are explicit end to end. The category field is still the old text field.

---

## Phase 4: User Story 2 - Pick a category from a list (Priority: P1)

**Goal**: The Category control shows the current choice; tapping it opens a paged list with "No
category", "New category…" and every category in use; one tap assigns it, no typing.

**Independent Test**: With notes in "Work" and "Home", open another note, tap Category, choose "Home",
save: the note is in the Home view without typing. (quickstart §3 rows 5–8 and 13; US2 scenarios 1–7.)

### Tests first (observe failing)

- [X] T024 [P] [US2] Extend `tests/bridge/test_notes_bridge.cpp` for `categoryChoices(current)`
  (`contracts/qml-bridge.md`): the list is `{"No category","none"}`, `{"New category…","new"}`, then
  `{name, "c:"+name}` per category in the store's case-insensitive order; with no categories only the
  two actions; categories that exist only on notes not yet synced are included; a deleted note's
  category is not; a current category that no stored note carries (unsaved choice) is inserted in its
  sorted place exactly once; a category literally named `none` or `new` gets the values `c:none`,
  `c:new` and cannot be confused with the actions; with 60 categories the first two entries are still
  the actions.
- [X] T025 [P] [US2] Extend `tests/unit/test_store_views.cpp` for `NoteStore::categories()`: includes
  the category of an `edited` note that never reached the server; excludes tombstoned (`deleted`)
  notes; a category disappears once its last note moves to another category or to none (FR-018);
  names that differ only in letter case are two entries; `/` names stay one flat entry.

### Implementation

- [X] T026 [US2] Add `Q_INVOKABLE QVariantList categoryChoices(const QString& current) const` to
  `bridge/include/notes_bridge.h` / `bridge/src/notes_bridge.cpp` per `contracts/qml-bridge.md`, built
  from `store_->categories()`; the labels "No category" and "New category…" go through `tr`. Make T024
  and T025 pass.
- [X] T027 [US2] In `qml/pages/EditorPage.qml` replace the Category `TextField` with an `InkButton`
  showing the current category or "No category" (elide long names), add a `category` property
  (`startCategory` stays), and a `ChoiceDialog` (reused unchanged, `title: qsTr("Category")`,
  `choices: bridge.categoryChoices(page.category)`, `current: page.category === "" ? "none" : "c:" +
  page.category`); `onChosen`: `none` → `category = ""`, `c:<name>` → `category = <name>` (strip the
  `c:` prefix), `new` → no-op until US3; hide the keyboard before opening and restore focus to the
  text area after closing; dismissing the dialog leaves the category as is; `dirty` and `save()` use
  `page.category`. Update the header comment.
- [X] T028 [US2] Run `ctest` in full and the app `--self-test`; all green.
- [x] T029 [MANUAL] [US2] On the reader walk through quickstart §3 rows 5–8 and 13 (30+ categories:
  paging reaches every entry; the two actions are on page 1). Record results in
  `specs/004-title-category-picker/checklists/device-results.md`.

**Checkpoint**: existing categories are chosen by tapping. "New category…" is listed but does nothing yet.

---

## Phase 5: User Story 3 - Define a new category from the picker (Priority: P2)

**Goal**: "New category…" opens a one-line entry; a valid name becomes the note's category and, once
the note is saved, is offered everywhere.

**Independent Test**: Open a note, Category → "New category…", type "Travel", Confirm, Save; open
another note: "Travel" is listed. (quickstart §3 rows 9–12; US3 scenarios 1–7.)

### Tests first (observe failing)

- [X] T030 [P] [US3] Extend `tests/bridge/test_notes_bridge.cpp` for `checkCategory(name)`
  (`contracts/qml-bridge.md`): `"  Travel  "` → `{ok:true, name:"Travel"}`; `""` and `"   "` →
  `{ok:false}` with the `category_empty` text ("Enter a category name."); 255 characters ok, 256 →
  `category_too_long` text; a name with a control character → the `category_control_chars` text;
  invalid UTF-8 → the `category_invalid` text; `"a/b"` ok and returned unchanged (one flat name); a
  name equal to an existing category returns that exact name (so assigning it creates no second
  spelling); `"work"` next to an existing `"Work"` is ok and different (case is significant);
  nothing is stored by the call.

### Implementation

- [X] T031 [US3] Add `Q_INVOKABLE QVariantMap checkCategory(const QString& name) const` to
  `bridge/include/notes_bridge.h` / `bridge/src/notes_bridge.cpp`: trim ASCII whitespace, reject empty
  with the new `category_empty` text, then `validate_category`; return `{ok, name, errorText}`. Add
  the `category_empty` text to the error table. Make T030 pass.
- [X] T032 [P] [US3] Create `qml/TextEntryDialog.qml` per `contracts/qml-bridge.md` (modal full-screen
  item like `qml/ConfirmDialog.qml`, white, `z: 1000`, no animation; `title`, `placeholder`,
  `errorText`; `open(initial)`, `close()`; signals `accepted(string text)` and `rejected()`; one-line
  `TextField` with `Qt.ImhNoPredictiveText`, Return = Confirm, the field kept clear of the on-screen
  keyboard like the editor's text, error line shown when `errorText` is non-empty, Confirm and Cancel
  as `InkButton`); register it in `qml/qml.qrc`.
- [X] T033 [US3] Wire it in `qml/pages/EditorPage.qml`: the `new` choice closes the picker and opens
  `TextEntryDialog` (`title: qsTr("New category")`, placeholder `qsTr("Name")`); `onAccepted`: call
  `bridge.checkCategory(text)`; on `ok` set `page.category = result.name`, close the dialog and
  return focus to the text area; else set `errorText = result.errorText` and keep the dialog open;
  `onRejected` leaves the category unchanged. A category picked or created here takes effect only on
  Save (it is part of `dirty`); leaving with Discard drops it (no store call is made before Save).
- [X] T034 [US3] Run `ctest` in full and the app `--self-test`; all green. Confirm with a quick
  bridge test run that a new category appears in `categoryChoices` of another note only after the
  first note was saved (extend T024's file if not yet covered: save a note with "Travel", then
  `categoryChoices("")` contains `c:Travel`; before saving it does not).
- [x] T035 [MANUAL] [US3] On the reader walk through quickstart §3 rows 9–12. Record results in
  `specs/004-title-category-picker/checklists/device-results.md`.

**Checkpoint**: all three stories work independently and together.

---

## Phase 6: Polish & cross-cutting concerns

- [X] T036 [P] Update the documents the feature supersedes: add a "superseded by feature 004" note to
  FR-019 and its clarification/assumption lines in `specs/001-pocketbook-notes-mvp/spec.md`; in
  `specs/001-pocketbook-notes-mvp/contracts/notes-api-v1.md` change the `POST` row (line 27) and the
  sentence "Title is never sent…" (line 36) to point to `specs/004-title-category-picker/contracts/
  notes-api-title.md`; update the `README.md` feature description of the editor (separate title,
  category picker) and the comment in `core/include/pbnotes/title.h` header ("the displayed title is
  ALWAYS the first non-empty line" is no longer true).
- [X] T037 Run `scripts/pre-merge.sh`: all tests, constitution checks (R5 no absolute paths, R6 no
  forbidden log keys), fixture anonymization check and secret scan green.
- [X] T038 Release 1.1.0 (constitution 1.3.0 requires a release task): raise `VERSION` to `1.1.0` and add
  a `## 1.1.0` entry at the top of `CHANGELOG.md` (explicit note title; category picker with new
  categories; notes edited before this version keep their titles; schema 3 migration). Run
  `scripts/check-version-bump.sh` as `scripts/pre-merge.sh` does. Tag `v1.1.0` after the merge.
- [x] T039 [MANUAL] Full quickstart §3 on the reader with the release package, including the
  schema-3 upgrade on a device database that has unsynced notes (§2 on real data: back the database up
  first, as the migration also does). Record in
  `specs/004-title-category-picker/checklists/device-results.md`; settle the **[VERIFY]** item of
  `research.md` R-3 there and in `research.md`.
- [X] T040 Update `specs/004-title-category-picker/checklists/requirements.md` notes: every FR-001..FR-023
  is covered by a task above; list the open [MANUAL] tasks.

---

## Dependencies & Execution Order

- Phase 1 (T001) first. Phase 2: T002 and T003 (tests, parallel) → T004 (needs T002) and T005 (needs
  T003); T004 and T005 touch different files but both edit `core/src/note_store.cpp`'s neighbors
  only through T004's `make_excerpt` call updates — do T004 before T005.
- US1 (Phase 3) needs Phase 2. Tests T006–T013 are independent files and run in parallel; they
  are all written (and seen failing) before T014. Implementation order: T014 → T015 → T016 → T017 →
  T018 → T019 → T020 → T021 → T022. T014 is the big mechanical step: do it before any other
  implementation task so the tree compiles again.
- US2 (Phase 4) needs US1's T020/T021 (`EditorPage.qml` and `NotesBridge` already carry the title).
  US3 (Phase 5) needs US2's `ChoiceDialog` wiring (T027); its bridge call T031 and dialog T032 can
  start earlier. All three stories edit `qml/pages/EditorPage.qml`, so those tasks are sequential.
- Polish needs the stories it documents or runs. T038 (version bump) comes last before the merge.

### Parallel opportunities

- Phase 2: T002 ‖ T003.
- US1 tests: T006 ‖ T007 ‖ T008 ‖ T009 ‖ T010 ‖ T011 ‖ T012 ‖ T013.
- US2 tests: T024 ‖ T025. US3: T030 ‖ T032 (the dialog needs no bridge call).
- Polish: T036 alongside T037 once the code is done.

## Implementation Strategy

- **MVP**: Phase 1 + Phase 2 + User Story 1 (T001–T022): the title is explicit, validated, synced
  and conflict-safe; the category field is the old text field. Stop and validate on the reader (T023).
- **Increment 2**: User Story 2 (pick an existing category). The "New category…" entry is visible
  but inert until Increment 3 — do not release between the two.
- **Increment 3**: User Story 3 (new categories), then Polish and the 1.1.0 release.
- Every task that changes behavior follows red → green; a task that adds a test also states that the
  test must be seen failing before the matching implementation task starts.
