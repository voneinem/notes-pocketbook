# Implementation Plan: Text Editing Tools and Title in the Reader

**Branch**: `feature/005-text-editing-reader-title` | **Date**: 2026-10-11 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/005-text-editing-reader-title/spec.md`

## Summary

The editor gains selection, Copy/Cut/Paste/Delete, Select all and one shared Undo/Redo history; the
reading page gains the title block and read-only selection with Copy. Built on the existing
architecture (Qt-free core, thin Qt bridge, QML UI), with no new library and no stored data:

- **One shared edit history** — a new Qt-free core class `EditHistory` records every change to the
  title, the text, the category and the favorite marker as a *step* (replacement of a range, or a
  value change). The editor does not tell it what the reader did: QML reports each new field value
  and the history *diffs* it against the value it already knows (common prefix/suffix), so typing, a
  pasted run, a cut, a keyboard auto-correction and an attachment marker line are all captured the
  same way. Typing is grouped by pause (≤ 1.5 s), run length (≤ 80 characters) and line breaks;
  compound commands (Cut, Delete, Paste over a selection) run in a group and become one step. Qt's
  per-field undo cannot be shared across a title field, a text area, a category and a star, so it is
  not used.
- **(Superseded 2026-10-11, see spec Clarifications: long-press menu and markers instead of a tools row.) Commands are buttons, not gestures** — a tools row in the editor (Undo, Redo, All, Extend,
  Copy, Cut, Paste, Del) whose buttons never take keyboard focus, so the on-screen keyboard stays
  open and the caret stays (FR-010). A selection starts by **press-and-hold on a word** (or *All*),
  and *Extend* (arm, then tap) stretches it to the tapped point. Both gestures are gated by a short
  device spike (research R-3) with a stated fallback.
- **Clipboard** — a small `ClipboardBridge` over `QClipboard`, falling back to an in-app buffer when
  the platform offers none. The SDK's public API has no clipboard (research R-4), so the delivered
  behavior on the reader is the in-app clipboard the spec allows; anything arriving through the
  platform clipboard is cleaned like untrusted input (`clean_pasted_text`, core). Title pastes are
  made single-line and length-checked in the same place.
- **Reader title block** — the reading page shows the stored title above the text in a bold, larger
  face, hidden for an empty title, unchanged text below (FR-019 – FR-025, Q5 answer: never
  de-duplicated). `openNote` reports `hasTitle` so the placeholder "(untitled)" is never shown there.
- **Reader selection and source copy** — reader text blocks become read-only `TextEdit` items so a
  character range can be shown selected (black on white, inverted). A selection spans *units* (the
  title, then each block); table rows, attachments and blank lines are selected whole. Copy puts
  the **source text** of the range on the clipboard (Q2 answer): the inline parser records, for every
  displayed byte, where it came from in the source line, and for every formatted element its full
  source extent, so `copy_source` (core, host-tested) can include the marks of elements that are
  completely selected and leave out the marks of partly selected ones (FR-031).

Research resolved every unknown (R-1 to R-12). Three device facts are **[SPIKE]** gates with a
fallback each (long-press, rich-text parity of the read-only `TextEdit`, platform clipboard); they
come first in `tasks.md` and do not change the design.

## Technical Context

**Language/Version**: C++17 (core, bridge, app); QML (Qt Quick 6.8) — unchanged.

**Primary Dependencies**: unchanged. No new library or asset. Qt Gui's `QClipboard` is already linked
(`cmake/qt_manual.cmake` links `Qt6Gui` for the device build).

**Storage**: none. No schema change, no setting, no file. The history and the clipboard buffer are
in memory only (FR-015); nothing about selection, history or clipboard is saved or synced.

**Testing**: the existing harness — host unit tests (CTest, one executable per `tests/unit/test_*.cpp`),
offscreen Qt bridge tests (`tests/bridge`, listed in `tests/CMakeLists.txt`), app `--self-test`,
manual device walkthrough. New and changed tests are listed in research R-12.

**Target Platform**: PocketBook InkPad One (Qt 6.8.x runtime, SDK build) and Linux x86-64 host —
unchanged.

**Project Type**: single application with a host-testable core library — unchanged.

**Performance Goals**: every command shows its result in under 1 s on the reader (SC-006). Per
keystroke the history costs one prefix/suffix scan of the changed field (about 0.2 ms for 100,000
characters on the host; budget on the reader 10 ms); the history is capped at 200 steps and
2,000,000 stored characters (always keeping the newest step); `copy_source` is linear in the
selected range.

**Constraints**: offline-capable (nothing here touches the network); pasted text is external input
and is cleaned before use (constitution III); no log line carries note text, copied text or a title
(event names and counts only, FR-033); the keyboard must not close when a tool button is pressed
(FR-010); selection and Copy in the reader must not write to the note or touch its modification
time (FR-027, SC-010).

**Scale/Scope**: 2 new core modules (`edit_history`, `text_clean`) + 1 extended core module
(`markdown`: source offsets and `copy_source`); 1 new bridge file with 2 classes (`EditSession`,
`ClipboardBridge`) + changed `reader_blocks` / `NotesBridge::openNote`; changed `EditorPage`,
`ReaderPage`, `BlockText`, `TableRowView`, `AttachmentView`; about 8 new test files and 3 existing
ones extended.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate (constitution) | Pre-research | Post-design | Evidence |
|---------------------|--------------|-------------|----------|
| I. Test-First | PASS | PASS | Every behavior with logic has a host seam: `EditHistory` (steps, grouping, undo/redo, caps), `clean_pasted_text` / `single_line`, `copy_source` and the parser offsets, `EditSession` / `ClipboardBridge`, `reader_blocks` / `openNote` fields. `tasks.md` must order each failing test before its code (R-12). QML wiring is covered by the device walkthrough and `--self-test` |
| II. Simplicity (YAGNI) | PASS with justified items | PASS with justified items | Four items in Complexity Tracking. Rejected: a system clipboard integration, a persistent undo history, drag handles, rich-text copy, find/replace, character-level selection of table cells |
| III. Security & Privacy | PASS | PASS | Pasted text is untrusted: UTF-8 validated, control characters dropped, 1,000,000-character cap (FR-032); the app reads the platform clipboard only on a Paste tap and writes it only on Copy/Cut (FR-033); no copied text, title or note text in logs, files or on the server; no new endpoint |
| IV. Integration tests at device boundaries | PASS | PASS | No boundary format changes (nothing is stored or sent). The one new "boundary" is the platform clipboard: the in-app fallback is tested on the host; the platform path is exercised on the reader (spike S-3) |
| V. Host-testable core | PASS | PASS | History, text cleaning and source copy live in `notes_core` without Qt; the bridge converts `QString` ↔ UTF-8/UTF-16 and QML only wires buttons |
| VI. No silent data loss | PASS | PASS | Undo/Redo and the commands change only the editor's working text; nothing is saved until Save, and leaving with a difference still asks (dirty = values differ from opening, so a full undo clears it, FR-017). The reader never writes. Cut/Delete are always undoable |
| Stack: C++17, Qt 6.8 Quick/QML, CMake, libcurl, SQLite | PASS | PASS | No stack change |
| SDK location only from `POCKETBOOK_SDK` / `-DSDK_PATH` | PASS | PASS | No build path added (new bridge header is added to the existing manual-moc list) |
| Both device and host build targets | PASS | PASS | All new code is in existing targets |
| Licensing: InkFeed attributed; notes-android reference-only | PASS | PASS | Behavior is specified from this project's spec; nothing copied from notes-android (its editor is not consulted) |
| Release version | PASS | PASS | A MINOR bump to 1.2.0 with a CHANGELOG entry is the last task of the feature |

No violations block planning.

## Project Structure

### Documentation (this feature)

```text
specs/005-text-editing-reader-title/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R-1 .. R-12
├── data-model.md        # Phase 1: in-memory entities, rules, source-offset model
├── quickstart.md        # Phase 1: host tests, spikes, device walkthrough
├── contracts/
│   ├── core-library.md  # EditHistory, clean_pasted_text/single_line, parser offsets, copy_source
│   └── qml-bridge.md    # EditSession, ClipboardBridge, openNote/reader_blocks fields, QML controls
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks — not created here)
```

### Source Code (repository root)

```text
core/include/pbnotes/
├── edit_history.h       # new: EditField, EditStep, EditApplied, EditHistory
├── text_clean.h         # new: clean_pasted_text, single_line
└── markdown.h           # + Span::src / el_begin / el_end, Block::src_begin / src_end, copy_source
core/src/
├── edit_history.cpp, text_clean.cpp                  # new
├── markdown.cpp, markdown_inline.cpp, markdown_table.cpp, markdown_internal.h   # record source offsets/extents; copy_source
bridge/
├── include/edit_bridge.h, src/edit_bridge.cpp        # new: EditSession, ClipboardBridge
├── include/reader_blocks.h, src/reader_blocks.cpp    # + "block" / "row" / "src" fields per entry
├── include/notes_bridge.h, src/notes_bridge.cpp      # openNote + hasTitle, titleText; readerCopy(...)
└── CMakeLists.txt                                    # + edit_bridge (host and manual moc lists)
app/main.cpp                                          # + context properties "editSession" (one, reset per editor) and "clipboard"
qml/
├── BlockText.qml        # read-only TextEdit, selection range, long-press word
├── TableRowView.qml, AttachmentView.qml              # selected (whole-unit) look
├── TitleBlock.qml       # new: the reader's title unit (selectable like a text block)
├── EditTools.qml        # new: the editor's tools row (never takes focus)
├── qml.qrc              # + TitleBlock.qml, EditTools.qml
└── pages/EditorPage.qml, pages/ReaderPage.qml
tests/
├── unit/                # test_edit_history, test_text_clean, test_markdown_source_map, test_copy_source (new)
├── bridge/              # test_edit_bridge (new), test_reader_blocks (+fields), test_notes_bridge (+hasTitle, readerCopy)
└── CMakeLists.txt       # + test_edit_bridge in the bridge list
VERSION, CHANGELOG.md    # 1.2.0 (last task)
```

**Structure Decision**: the repository's existing layout (`core/`, `bridge/`, `qml/`, `tests/`) is
kept; no new module. The rules that decide results (history, cleaning, source copy) are core
functions with host tests; the bridge only converts types and holds the session object; QML holds
the gestures and which button is enabled.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| New core class `EditHistory` (≈250 lines) | FR-013 needs one history over four unrelated controls (title field, text area, category, favorite), steps for compound commands, and "undo to opened state clears the unsaved prompt" | Qt's built-in `TextEdit.undo()` is per document: it cannot order a category choice between two text edits, cannot span the title and the text, and its grouping is not testable on the host |
| Source offsets in the Markdown parser (`Span::src`, element extents, `Block::src_*`) | FR-031 / Q2: copying from the reading page must return the *source* text, with the marks of fully selected elements and none for partly selected ones, from an HTML rendering that dropped the marks | Greedy re-alignment of displayed text against the source line mis-maps when a dropped mark equals a content character (`**\*` cases) and cannot find the extent of an element; copying whole lines would contradict US4 scenario 10 |
| Reader text blocks become read-only `TextEdit` instead of `Text` | Only `TextEdit` can show a character range selected and return `positionAt` / `selectWord`; `Text` has neither | Drawing our own highlight over `Text` needs per-glyph geometry that `Text` does not expose; block-level (whole paragraph) selection would break FR-026 / US4-10 (partial selection) |
| Two small bridge classes (`EditSession`, `ClipboardBridge`) | The history must be a long-lived object per editor session, and the clipboard needs cleaning plus a fallback when the platform has none | Putting both in `NotesBridge` (already ~580 lines) mixes unrelated concerns; doing it in QML makes the rules untestable on the host |
