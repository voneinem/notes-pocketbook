# Implementation Plan: Better Markdown Support and Slash Menu

**Branch**: `feature/006-markdown-slash-menu` | **Date**: 2026-10-11 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/006-markdown-slash-menu/spec.md`

## Summary

The reading page learns fenced code blocks, clearer headings/bold and five more formats (quote, info
box, strikethrough, underline, collapsible details); the editor gains a "/" menu and a "//" date-time
picker. Built on the existing architecture (Qt-free core, thin Qt bridge, QML UI) with **no new
library and nothing stored**:

- **Parser keeps one block per source line.** The reader's selection, source copy and paging all
  address "units = blocks = source lines" (feature 005). So a fenced block is *not* one big block:
  the fence lines become `Fence` blocks (the box's top and bottom edge), the lines between them
  `Code` blocks (literal, fixed-width), and a post-pass marks which block is first/last of its box so
  the QML draws one continuous frame. Quote and info box work the same way (a `quote` attribute plus
  first/last flags); `<details>` adds a `Summary` block, hidden `Marker` blocks for the structural
  lines, and a `detailsId` on the content blocks. `copy_source`, `toggle_task_item`, selection and
  paging therefore need only small additions, not a new model.
- **One fence rule, shared.** Today three places toggle on "```" (`parse_markdown`,
  `find_attachment_refs`, `tokenize_attachment_refs`) and a fourth ends tables on it. They move to
  one small helper (`md::` fence opener/closer: backtick or tilde, three or more, ≤ 3 spaces of
  indent, closer of the same kind and at least as long), so attachments and task toggles ignore
  exactly what the reader shows as code.
- **Inline `~~strike~~` and `<u>underline</u>`** become two boolean flags on `Span` (next to the
  existing bold/italic kind), so they combine with every other format and survive the link and table
  paths that rebuild spans. The bridge emits `<s>` / `<u>`.
- **Headings and bold**: parsing is unchanged (it already handles `#`..`######` and `**`); the
  work is *display*: wider size steps between levels 1–3, and a "keep with next" page rule so a
  heading is never the last thing on a page (QML-built interval, existing `page_target`).
- **Slash menu logic lives in the core** (`pbnotes/slash.h`, Qt-free, host-tested): *detect* a typed
  "/" or "//" from the before/after text and caret (boundary rule, single typed character,
  not a paste/undo), *expand* a chosen entry into a replacement with its caret position (new-line
  rule for block entries), *format* a date-time, *days in month*. QML only shows the lists and
  applies the replacement.
- **One undo step per insertion** without changing `EditHistory`: when a trigger is detected the
  editor opens a history group *before* reporting the "/" (`begin_group` then `observe`), so the
  typed "/" (or "//") and the later replacement become one step; dismissing the menu ends the group
  (the "/" then is an ordinary step). Pinned by new tests on the existing API.
- **Three small QML components**: `SlashMenu` (a two-column list of the twelve entries in order),
  `DateTimePicker` (year/month/day/hour/minute steppers, only valid days), `EmojiPicker` (a fixed
  grid; the list is a constant in the bridge so a host test can prove every entry has a glyph in the
  bundled emoji font). Back closes only the open popup (a `backHandled()` hook before `Main.qml`
  pops the page).

Research resolved every unknown (R-1 to R-12). Four device facts are **[SPIKE]** gates with a
fallback each (monospace face, strike/underline on e-ink, popup above the keyboard and Back,
date-time picker tap targets); they come first in `tasks.md`.

## Technical Context

**Language/Version**: C++17 (core, bridge, app); QML (Qt Quick 6.8) — unchanged.

**Primary Dependencies**: unchanged. No new library. *Conditional*: if spike S-1 finds no
fixed-width face on the reader, one small OFL monospace font is bundled the way the emoji font is
(complexity item C-3 below); otherwise nothing is added.

**Storage**: none. No schema change, no setting, no file, no sync field. Notes stay plain Markdown
text; the new formats are display of, and insertion of, ordinary text (FR-019, FR-020).

**Testing**: the existing harness — host unit tests (CTest, one executable per `tests/unit/test_*.cpp`),
offscreen Qt bridge tests (`tests/bridge`), QML page tests with injected mouse events (`tests/qml`),
app `--self-test`, manual device walkthrough. New and changed tests are listed in research R-12.

**Target Platform**: PocketBook InkPad One (Qt 6.8.x runtime, SDK build) and Linux x86-64 host —
unchanged.

**Project Type**: single application with a host-testable core library — unchanged.

**Performance Goals**: opening and paging a note with 500 code lines is not slower than a note of
500 paragraph lines (SC-008). This holds by construction: a fenced line is already one block today.
The slash detection runs once per text change on the changed range only (a diff of the old and new
text that the history already computes), under 1 ms for 100,000 characters on the host.

**Constraints**: offline (nothing here touches the network, FR-021); no note text in logs (event names
and counts only); no new colors (monochrome: frames are borders, kinds are labels); e-ink: no
animation, no kinetic scrolling; the popups must never take keyboard focus away from the text area
(the keyboard stays open) and must fit above the keyboard.

**Scale/Scope**: 1 new core module (`slash`) + 1 shared helper (fences) + extended `markdown`
(blocks, spans, source mapping); 1 new bridge class (`SlashBridge`, in `edit_bridge.*`) + extended
`reader_blocks`; 3 new QML components, changed `BlockText`, `ReaderPage`, `EditorPage`, `Main`;
about 5 new test files and 6 existing ones extended.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate (constitution) | Pre-research | Post-design | Evidence |
|---------------------|--------------|-------------|----------|
| I. Test-First | PASS | PASS | Every behavior with logic has a host seam: fence rule, block/frame/details parsing, inline flags, `copy_source` for the new blocks, slash detect/expand/timestamp/days-in-month, history-group usage, bridge fields, emoji coverage. `tasks.md` must order each failing test before its code (R-12). QML wiring is covered by mouse-event page tests and the device walkthrough |
| II. Simplicity (YAGNI) | PASS with justified items | PASS with justified items | Three items in Complexity Tracking. Rejected: syntax coloring, inline code rendering, nested quotes, type-to-filter in the menu, a date-format setting, emoji search/skin tones, a general container model, storing anything |
| III. Security & Privacy | PASS | PASS | Rendering reads note text only; the `<u>`, `<s>` and `<details>` forms are *recognized*, never passed through: the bridge builds its own markup from spans and escapes all text, so no note content becomes live HTML. No link in a code block can be followed (FR-002). No new endpoint, file or log field |
| IV. Integration tests at device boundaries | PASS | PASS | No boundary format changes (text is stored and synced byte for byte, FR-019). A round-trip test with a note holding every new format proves save/sync does not alter it |
| V. Host-testable core | PASS | PASS | Parsing, detection, expansion, date formatting live in `notes_core` without Qt; the bridge converts `QString` ↔ UTF-16 and QML only lays out and applies results |
| VI. No silent data loss | PASS | PASS | Insertions only add text and are one undo step; the reader never writes; collapsed details only hide *display* (text kept, copy includes it). A "/" typed is never blocked and never lost (FR-009) |
| Stack: C++17, Qt 6.8 Quick/QML, CMake, libcurl, SQLite | PASS | PASS | No stack change |
| SDK location only from `POCKETBOOK_SDK` / `-DSDK_PATH` | PASS | PASS | No build path added (new QML files go in the existing `qml.qrc`; the one new header-less bridge class lives in an existing file) |
| Both device and host build targets | PASS | PASS | All new code is in existing targets |
| Licensing: InkFeed attributed; notes-android reference-only | PASS | PASS | Syntax follows public Markdown/GitHub-alert conventions; nothing copied from notes-android. If a monospace font is bundled (C-3), its license is added to `NOTICE`/`licenses/` like the emoji font |
| Release version | PASS | PASS | A MINOR bump to 1.3.0 with a CHANGELOG entry is the last task of the feature |

No violations block planning.

## Project Structure

### Documentation (this feature)

```text
specs/006-markdown-slash-menu/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R-1 .. R-12, spikes S-1 .. S-4
├── data-model.md        # Phase 1: block/span fields, slash entries, picker state (nothing stored)
├── quickstart.md        # Phase 1: host tests, spikes, device walkthrough
├── contracts/
│   ├── core-library.md  # markdown.h additions, fences, slash.h
│   └── qml-bridge.md    # reader block fields, SlashBridge, history-group use, QML components
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks — not created here)
```

### Source Code (repository root)

```text
core/include/pbnotes/
├── markdown.h           # + BlockKind::Fence/Code/Callout/Summary/Marker, Block quote/frame/details fields,
│                        #   Span::strike/underline
└── slash.h              # new: SlashEntry list, detect_slash, expand_slash, replace_range, format_timestamp, days_in_month
core/src/
├── markdown.cpp         # fences (shared helper), quote/info/details line parsing, frame post-pass
├── markdown_inline.cpp  # ~~ and <u> spans; flags kept by add_span / cell_spans / links
├── markdown_table.cpp   # ends_table uses the shared fence test (and ">" / details markers)
├── markdown_copy.cpp    # Fence / Marker / Callout copy whole lines; Code and Summary like text blocks
├── markdown_internal.h  # fence helper declarations
└── slash.cpp            # new
bridge/
├── include/edit_bridge.h, src/edit_bridge.cpp   # + SlashBridge (entries, detect, expand, timestamp, emoji list)
└── include/reader_blocks.h, src/reader_blocks.cpp   # new kinds/fields, <s>/<u> markup, unsplittable for code lines
app/main.cpp             # + context property "slash"
qml/
├── BlockText.qml        # code/fence/callout/summary/quote frames, mono font, heading scale, details marker
├── SlashMenu.qml        # new: two-column list popup, never takes focus
├── DateTimePicker.qml   # new: steppers, valid days only, Insert / Cancel
├── EmojiPicker.qml      # new: fixed grid, Cancel
├── qml.qrc              # + the three components
├── Main.qml             # Back asks the page first (backHandled)
└── pages/ReaderPage.qml, pages/EditorPage.qml   # details open-state and keep-with-next; slash/date/emoji flow
tests/
├── unit/                # test_markdown_fence, test_markdown_blocks, test_slash (new); test_markdown,
│                        #   test_markdown_source_map, test_copy_source, test_markdown_attachments,
│                        #   test_markdown_toggle, test_edit_history (extended)
├── bridge/              # test_reader_blocks (+fields), test_edit_bridge (+SlashBridge, emoji coverage)
├── integration/         # test_edge_cases (+ round-trip of every new format)
└── qml/                 # reader_blocks.qml, editor_slash.qml (new, mouse events)
VERSION, CHANGELOG.md    # 1.3.0 (last task)
```

**Structure Decision**: the repository's existing layout (`core/`, `bridge/`, `qml/`, `tests/`) is
kept; no new module or library. Rules that decide results (what is a fence, a quote, an info box,
a details section; when "/" opens the menu; what an entry inserts; the date text) are core
functions with host tests; the bridge converts types; QML holds layout, the open/closed state of
details sections, and the popups.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| C-1. New core module `slash` (≈200 lines) | FR-007..FR-014 have many precise rules (boundary, new-line rule per entry, caret per entry, valid days, leap years, 24-hour text) that must be tested without a device | Putting them in QML makes them untestable on the host (constitution I, V) |
| C-2. Block-model growth: 5 block kinds, quote/code/details fields, first/last post-pass | Containers (code box, quote, info box, details) must look continuous while selection, copy and paging keep working per source line | One block per container breaks partial selection and `copy_source` (feature 005 scenarios US4-8..10) and needs a second addressing scheme for characters inside; a generic nested-container tree is far more than five formats need |
| C-3. *Conditional* bundled monospace font | FR-001 needs fixed-width type; if the reader has no fixed-width face, code alignment (YAML) is lost | Using a proportional face would break the main use case (indentation). Only taken if spike S-1 fails; adds a font file, a `NOTICE` entry and a license text exactly as the emoji font did |
| C-4. Three new QML components | A list popup, a date-time picker and an emoji grid are different shapes; none exists | Reusing `SelectionMenu` (a row of buttons) cannot show a 12-entry ordered list; reusing `ChoiceDialog` is modal and hides the text (and the keyboard) |
