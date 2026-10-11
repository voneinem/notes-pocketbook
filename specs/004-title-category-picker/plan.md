# Implementation Plan: Explicit Note Title and Category Picker

**Branch**: `feature/004-title-category-picker` | **Date**: 2026-10-11 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/004-title-category-picker/spec.md`

## Summary

The note's title stops being a guess and becomes a stored, edited, synced field; the category stops
being free text in the editor and becomes a choice from the categories that already exist. Built on
the existing architecture (Qt-free core, thin Qt bridge, QML UI), with no new library:

- **Title is data, not derivation** — the `notes.title` column already exists. Every write path that
  recomputed it from the text (`create_local`, `save_local`, the attachment upload that rewrites a
  token, conflict copies) now takes or keeps the stored title. `create_local` / `save_local` gain a
  `title` argument, validated and trimmed in the core (`validate_title`). Category, favorite and
  checkbox edits leave the title alone.
- **Title travels to the server** — `NoteDraft` gains `title` and every `POST`/`PUT` carries it, so
  the server never derives one from the text (FR-008). The title the server answers with is stored
  as today (`title_for`), so a title the server adjusted is what the reader sees afterwards.
- **No silent loss of a title edit** — the conflict check treats a different title as a difference
  (a title-only edit on both sides keeps both versions), and a conflict copy is named by appending
  the conflict suffix to the stored *title*; the copy's text is no longer touched.
- **Preview line** — `make_excerpt` gets the title and leaves out the first line only if it merely
  repeats the title (FR-023), so nothing the reader typed vanishes from the list.
- **Category picker** — the editor's Category row becomes a button that opens the existing paged
  `ChoiceDialog` with "No category", "New category…" and the categories of all notes on the reader;
  "New category…" opens a small new `TextEntryDialog` (type, validate, confirm or cancel). Names are
  checked by the existing `validate_category` after trimming; no new storage (categories stay
  derived from the notes).
- **One-time data step** — schema version 3 repairs rows written by older versions so the first
  push never sends a guess: it strips the heading marker from the titles of unsynced notes (what
  the list showed) and recomputes the preview lines.

Research resolved every unknown (R-1 to R-9). One fact is a **[VERIFY]** item with a stated
fallback (how the real Notes server treats an explicit title); it does not block the design.

## Technical Context

**Language/Version**: C++17 (core, bridge, app); QML (Qt Quick 6.8) — unchanged.

**Primary Dependencies**: unchanged. No new library or asset.

**Storage**: the existing SQLite database; **no new column or table**. Schema version 2 → 3 is a
data-only migration (R-6), taken with the existing pre-migration backup.

**Testing**: the existing harness (host unit/integration/contract tests with CTest, offscreen Qt
bridge tests, app `--self-test`, manual quickstart on the device). New and changed tests are listed
in research R-9 and quickstart §1.

**Target Platform**: PocketBook InkPad One (Qt 6.8.x runtime) and Linux x86-64 host — unchanged.

**Project Type**: single application with a host-testable core library — unchanged.

**Performance Goals**: opening the category list or saving a note is not slower than today for a
store of a few thousand notes (the category list is one indexed `GROUP BY`, already used for the
Views list); the schema migration finishes in under 5 s for 5,000 notes (the preview recompute is
the only per-row work).

**Constraints**: offline-capable; titles and category names are external input and validated before
use (constitution III); no log line carries a title or category (counts and event names only); the
title is always sent explicitly, never guessed; no change to the conditional-write rules
(`If-Match` on PUT, conditional GET before DELETE).

**Scale/Scope**: 1 changed core module group (title, store, api draft, sync draft), 1 changed bridge
class (`NotesBridge`: 3 changed calls, 1 new), 1 changed page (`EditorPage`), 1 new component
(`TextEntryDialog`), 1 schema step; about 25 existing test files touched mechanically for the new
`title` argument.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate (constitution) | Pre-research | Post-design | Evidence |
|---------------------|--------------|-------------|----------|
| I. Test-First | PASS | PASS | New behavior has host-testable seams (`validate_title`, store edits, conflict identity, excerpt, migration, draft JSON, bridge calls); `tasks.md` must order each failing test before its code; list in R-9 |
| II. Simplicity (YAGNI) | PASS with justified items | PASS with justified items | Two items in Complexity Tracking (schema v3 data step, new `TextEntryDialog`). Rejected: a persistent category table, category rename/delete, a `title_explicit` flag column, a fuzzy/auto-complete search box, tag chips, deriving a *suggested* title |
| III. Security & Privacy | PASS | PASS | Title and category name are validated (length, control characters, UTF-8) before storing or sending; no new network endpoint, no new data class leaves the device (the title is note data already sent as content); logs stay free of titles and categories |
| IV. Integration tests at device boundaries | PASS | PASS | Contract tests for the create/update body (title present, correct JSON escaping) and for the server's answer with an adjusted title; migration test with a version-2 fixture database (anonymized) |
| V. Host-testable core | PASS | PASS | All title/category rules, the migration and the conflict naming live in `notes_core`; the bridge and QML only call them |
| VI. No silent data loss | PASS | PASS | A title-only difference is a conflict, never an overwrite (R-4); the migration backs the database up first and rolls back as one transaction; a save refused for a bad title keeps everything typed |
| Stack: C++17, Qt 6.8 Quick/QML, CMake, libcurl, SQLite | PASS | PASS | No stack change |
| SDK location only from `POCKETBOOK_SDK` / `-DSDK_PATH` | PASS | PASS | No build path added |
| Both device and host build targets | PASS | PASS | All new code is in existing targets |
| Licensing: InkFeed attributed; notes-android reference-only | PASS | PASS | Behavior is specified from this project's spec; nothing copied from notes-android |

No violations block planning.

## Project Structure

### Documentation (this feature)

```text
specs/004-title-category-picker/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R-1 .. R-9
├── data-model.md        # Phase 1: Note title/category fields, rules, migration
├── quickstart.md        # Phase 1: host tests, migration check, device walkthrough
├── contracts/
│   ├── core-library.md     # validate_title, store edit signatures, excerpt, conflict naming
│   ├── notes-api-title.md  # the explicit title on POST/PUT and the answer
│   └── qml-bridge.md       # saveNote/loadForEdit/categoryChoices/checkCategory, dialog contract
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks — not created here)
```

### Source Code (repository root)

```text
core/include/pbnotes/
├── title.h              # + strip_heading_marker; make_excerpt(text, title); - with_title_suffix, - derive_title use in writes
├── note_store.h         # + validate_title, EditError::InvalidTitle; create_local/save_local take a title; PushSnapshot.title; kSchemaVersion = 3
├── api_client.h         # NoteDraft.title
└── types.h              # Note.title comment (stored, explicit)
core/src/
├── title.cpp, note_store.cpp, note_store_attachments.cpp   # writes keep/take the stored title; conflict identity and copy naming; migration step
├── api_client.cpp       # draft_json adds "title"
└── sync_engine.cpp      # draft_of / snapshot carry the title
bridge/
├── include/notes_bridge.h, src/notes_bridge.cpp    # loadForEdit(+title), saveNote(+title), categoryChoices, checkCategory, display_title (no heading strip)
qml/
├── TextEntryDialog.qml  # new: one-line modal entry with Confirm/Cancel and an error line
├── qml.qrc              # + TextEntryDialog.qml
└── pages/EditorPage.qml # Title field, Category button + picker + new-category entry
tests/
├── unit/                # test_title, test_store_edit, test_category_validation(+title), test_conflict_title, test_conflict_atomic, test_store_migration_v3 (new), test_store_pull, …
├── contract/            # test_api_write (title in body, adjusted title in answer)
├── integration/         # test_sync_push, test_sync_conflict (title-only conflict), test_edge_cases
└── bridge/              # test_notes_bridge (title round trip, picker data, category check)
```

**Structure Decision**: the repository's existing layout (`core/`, `bridge/`, `qml/`, `tests/`) is
kept; no new module. The picker data and the new-name check are bridge calls over core rules, so
the rules stay host-tested and the QML stays free of logic.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Schema version 3 (data-only migration with backup) | Notes edited before this feature and still unsynced carry a title that is the raw first line (`## Plan`); pushing it as an explicit title would put the heading marker into the server's file name, and their preview lines were cut at the old rule | Sending no title for those notes needs a per-row "title is a guess" flag (a new column plus branches in push and conflict code); doing nothing sends a guess, which FR-008 forbids |
| New `TextEntryDialog.qml` (about 70 lines) | "New category…" needs a one-line text entry with Confirm/Cancel and an inline error; no such component exists | Reusing the editor's text field would put validation messages and focus handling into the editor page; a native dialog is not available on the reader |
