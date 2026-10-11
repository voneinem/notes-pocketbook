# Quickstart: validating Better Markdown Support and the Slash Menu

Validation guide only; implementation details live in `tasks.md`. Contracts:
[core-library](contracts/core-library.md), [qml-bridge](contracts/qml-bridge.md). Data:
[data-model](data-model.md). Decisions: [research](research.md). Build, packaging and device
prerequisites are those of `specs/001-pocketbook-notes-mvp/quickstart.md` §1–§3.

## 1. Host build and tests (no device)

```bash
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host
ctest --test-dir build-host --output-on-failure
scripts/pre-merge.sh
```

Expected: all suites pass offline. New tests (research R-12): `test_markdown_fence`,
`test_markdown_blocks`, `test_slash` (core, no Qt); extended: `test_markdown`,
`test_markdown_source_map`, `test_copy_source`, `test_markdown_attachments`, `test_markdown_toggle`,
`test_edit_history`, `test_reader_blocks`, `test_edit_bridge`, `test_edge_cases`; QML page tests
`reader_blocks.qml`, `editor_slash.qml`. Each is written first and observed failing before its
implementation (constitution I). `app_self_test` must still pass.

## 2. Spikes on the reader (before the dependent work)

| # | Check | Pass | Fallback |
|---|-------|------|----------|
| S-1 | A note with a fenced block of `iiii` / `MMMM` / indented lines, and a paragraph with **bold** next to regular text | the code glyphs have equal width, indentation lines up, bold is visibly heavier | bundle one OFL monospace font (research R-7, complexity C-3); bold: if not heavier, use the font's bold file or a doubled stroke — decide with the author |
| S-2 | `~~strike~~` and `<u>under</u>` in a paragraph and a list item, at all three reading sizes | the line through / under the text is visible and does not hide the glyphs | none expected; if the strike is invisible, draw it as `<s>` with a thicker font weight, report |
| S-3 | In the editor with the keyboard shown: open a throw-away popup of 6 two-column rows; press the device's Back | the popup is fully above the keyboard, every button taps, Back reaches `onBackRequested` | one-column scrolling list; a visible *Close* entry (research R-10) |
| S-4 | The 48 emojis in the grid, and a `DateTimePicker` mock with five `▲▼` columns | all emojis show as glyphs (no boxes) and each is tappable, steppers are reliably hit with a finger | drop the failing emojis from the list; enlarge steppers |

Record each result in the feature's walkthrough note; they change the design only where a fallback
says so.

## 3. Host scenarios (what the tests prove)

| Scenario | Spec | Where |
|----------|------|-------|
| `` ```yaml `` … `` ``` `` is Fence/Code…/Fence; tilde fences; 4 backticks need a 4+ closer; unclosed runs to the end; empty block | US1-1, 1-6, 1-9, 1-10 | `test_markdown_fence` |
| `# `, `**`, `- `, `[a](http://b.example)`, `\| a \| b \|` inside code stay literal; no link in code | US1-3 | `test_markdown_fence`, `test_reader_blocks` |
| A task box or attachment reference inside a fence is not a task / attachment; `toggle_task_item` ignores it | FR-020 | `test_markdown_toggle`, `test_markdown_attachments` |
| `#`..`######` need a space; `#tag` is text; `**x**` in heading/list/task/cell; unmatched `**` literal | US2-1..5 | `test_markdown_blocks` |
| Quote, info kinds (case-insensitive), lone `>`, quote ends at a plain line; fence inside a quote | US4-1, 4-2, edge cases | `test_markdown_blocks` |
| `<details>` with / without summary, `open`, unclosed, nested stays text | US4-5, 4-6, 4-10 | `test_markdown_blocks` |
| `~~x~~`, `<u>x</u>`, combined with bold and links, in table cells; unclosed literal | US4-3, 4-4, 4-7 | `test_markdown_blocks`, `test_reader_blocks` |
| Copy across code / quote / callout / collapsed section; partial range inside code; whole fence line | US1-8, 4-9, FR-019 | `test_copy_source`, `test_markdown_source_map` |
| `detect_slash`: start of text, after `\n`/space/tab → menu; after letter, digit, `:`; `https://`; `1/2`; paste of 2 chars; delete → none; `//` with menu open → datetime | US3-1, 3-2, 5-1, 5-8 | `test_slash` |
| Each of the 11 text entries: exact text, caret, new-line rule before/after; re-parsed block kinds match | US3-3, 3-10 | `test_slash` |
| `format_timestamp`, `days_in_month` (28/29/30/31, 2100 not leap) | US5-3, 5-5 | `test_slash` |
| A group opened before the first `/` and ended by the replacement is one undo step; a dismissed group is an ordinary step | US3-6, FR-011 | `test_edit_history` |
| Every emoji of the grid has a glyph in the bundled font | US5-9 | `test_edit_bridge` |
| A note with every new format saves, syncs and reloads byte for byte | FR-019, SC-006 | `test_edge_cases` |
| In the editor: `/` → menu; entry → text + caret; Undo → note as before; `a/` → nothing; `//` → picker; Cancel keeps `//`; Title never; Back closes only the popup | US3, US5 | `editor_slash.qml` |
| On the reading page: summary tap opens/closes; closed content skipped; heading interval extended | US4-5, FR-005 | `reader_blocks.qml` |

## 4. Device walkthrough (InkPad One)

Prerequisite: spikes S-1..S-4 done; a disposable account or local notes: one note with a `yaml`
block (nested, indented, a `**not bold**` and a `# not heading` line, a 200-character line), a
fence without closer, `~~~` fence; one note with `#`..`######`, bold in paragraph/heading/list/task/
table cell and a stray `**`; one note with a quote, each info kind, `~~`, `<u>`, a details section
(closed, one `open`, nested, unclosed). Install per the standing instructions for the mounted reader,
then:

| # | Step | Expected |
|---|------|----------|
| 1 | Open the code note | the block stands apart (border/shade), fixed-width, indentation intact, fence lines and `yaml` not shown as text, markup shown literally, the long line wraps (US1-1..4, 1-7) |
| 2 | Page down through a long block | a line is never cut across a page break (US1-5) |
| 3 | Select across the block and Copy; paste into a new note's editor | the fences and code appear exactly as written (US1-8) |
| 4 | Open the headings note, then A- / A+ | four+ distinct heading sizes in order; bold clearly heavier everywhere; stray `**` as typed; relations kept when resizing (US2-1..6) |
| 5 | Put a heading at a page end (page through) | the heading starts the next page with its text (US2-7) |
| 6 | Open the formats note | quote with bar; info boxes labelled by kind in black and white; strikethrough and underline visible; details show only their summary; tap opens/closes; `open` starts open; unclosed runs to the end (US4-1..10) |
| 7 | Edit a new note: type `/` at a line start | the list of twelve appears above the keyboard; the keyboard stays (US3-1, 3-9) |
| 8 | Choose *Code block*, type `a: 1`; Undo | the block is gone in one step; Redo brings it back (US3-3, 3-6) |
| 9 | Type `see a/` and `https://x` and `1/2` | no list appears (US3-2, SC-005) |
| 10 | Type `/` then `x` | the list closes, `/x` stays as typed (US3-4) |
| 11 | Type `/`, tap elsewhere; then `/`, press Back | the list closes; the editor stays open on Back (US3-4, 3-5) |
| 12 | In the Title field type `/` and `//` | nothing opens (US3-7) |
| 13 | Go through every entry once | each inserts its Markdown with the caret where expected; block entries start a new line when typed mid-line (US3-3, 3-10) |
| 14 | Type `//` | the date-time picker opens at now; change day/hour; Insert → `YYYY-MM-DD HH:MM`; Undo removes it; set Feb 31 is impossible; Cancel keeps `//` (US5-1..7) |
| 15 | Choose *Emoji*, tap one | inserted in place of the `/`; Cancel leaves the `/` (US5-9) |
| 16 | Save, Sync, open on another device or the server's web UI | texts identical; formats shown as that app shows them (US5-10, SC-006) |
| 17 | Rotate with a popup open | the popup closes, the text is unchanged (edge case) |

Record the device results and any change they cause (heading scale, grid size) in the walkthrough
note, as for features 004 and 005.
