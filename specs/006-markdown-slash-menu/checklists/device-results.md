# Device results and coverage: Better Markdown Support and Slash Menu

Filled in during and after the device walkthrough ([quickstart](../quickstart.md) §2 and §4). On 2026-10-11 the author
installed release 1.3.0 on the InkPad One, tried it and reported "works like a charm". That is an informal approval of
the whole walkthrough, not a row-by-row record: no per-spike measurements (glyph widths, tap success rates) were noted.
The host suite (88 tests) and the ARM build passed before the install.

## Open manual tasks

| Task | What to check | Result |
|------|---------------|--------|
| T009 | Spike S-1: fixed-width glyphs, indentation, bold heavier than regular | approved informally by the author on the reader (2026-10-11) |
| T017 | Bundled monospace font — only if T009 fails | not needed: code showed fixed-width |
| T023 | Heading sizes and keep-with-next on the panel (SC-004) | approved informally by the author on the reader (2026-10-11) |
| T029 | Spike S-3: the 2-column list above the keyboard, Back reaches `onBackRequested` | approved informally by the author on the reader (2026-10-11) |
| T037 | Walkthrough rows 7–13 and 17 | approved informally by the author on the reader (2026-10-11) |
| T043 | Spike S-2: strike and underline visible at the three reading sizes | approved informally by the author on the reader (2026-10-11) |
| T052 | Walkthrough row 6: quote, info boxes, details by finger | approved informally by the author on the reader (2026-10-11) |
| T056 | Spike S-4: the 48 emojis as glyphs; picker buttons hit with a finger | approved informally by the author on the reader (2026-10-11) |
| T063 | Walkthrough rows 14–15 and 17 | approved informally by the author on the reader (2026-10-11) |
| T069 | Full walkthrough with the release package (17 rows), incl. landscape | approved informally by the author on the reader (2026-10-11) |

## Requirement coverage (every FR has tests or a manual task)

| Requirement | Where |
|-------------|-------|
| FR-001..FR-004 (code blocks) | T004–T018: `test_markdown_fence`, `test_copy_source`, `test_reader_blocks`, `qml_reader_blocks`; device T009, T069 |
| FR-005, FR-006 (headings, bold) | T019–T024: `test_markdown_blocks`, `test_paging`, `qml_reader_blocks`; device T023 |
| FR-007..FR-011 ("/" menu, one undo step) | T025–T037: `test_slash`, `test_edit_history`, `test_edit_bridge`, `qml_editor_slash`; device T029, T037 |
| FR-012..FR-015 ("//" picker, emoji grid) | T053–T063: `test_slash`, `test_edit_bridge` (incl. emoji glyph coverage), `qml_editor_slash`; device T056, T063 |
| FR-016..FR-018 (quote, info box, strike, underline, details) | T038–T052: `test_markdown_blocks`, `test_copy_source`, `test_reader_blocks`, `qml_reader_blocks`; device T043, T052 |
| FR-019 (text unchanged) | T044, T064: `test_edge_cases` (byte-for-byte round trip), `test_markdown_golden` |
| FR-020 (nothing else changes) | T002, T005, T064: `test_markdown_golden`, the existing suites |
| FR-021 (offline, nothing sent) | T065: review — no network code, no log with text |

## Deviations worth knowing

- T030 also implemented `format_timestamp` and `days_in_month` (planned for T057); their tests (T053) were written
  after the code, not before. The behavior is pinned by `test_slash` and `test_edit_bridge` nonetheless.
- `EditHistory::undo()` now ends an open group before looking at the stack (it returned nothing with a group open and
  an empty stack); the editor relies on it when the long-press menu opens over a typed "/".
- `toggle_task_item` also toggles a task box inside a quote (`> - [ ] x`), so the box the reader draws there works.
- The "[!NOTE] header" is recognized only as the first line of a quote group (the tasks and research said both that and
  the opposite; the first reading is the one in the code and in the docs now).
