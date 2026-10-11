# Quickstart: validating text editing tools and the reader title

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

Expected: all suites pass offline. New tests (research R-12): `test_edit_history`,
`test_text_clean`, `test_markdown_source_map`, `test_copy_source` (core, no Qt) and
`test_edit_bridge` (bridge); extended: `test_reader_blocks`, `test_notes_bridge`. Each is written
first and observed failing before its implementation (constitution I). `app_self_test` must still
pass with the changed pages.

## 2. Spikes on the reader (before the dependent work)

| # | Check | Pass | Fallback |
|---|-------|------|----------|
| S-1 | (Superseded: gestures are now `MouseArea.pressAndHold`; check in the editor and the reading page that a held finger selects the word.) | the word is selected, the keyboard stays | report it: no fallback button exists any more |
| S-2 | The reading page with a note holding a heading, bold/italic, a link, a task list and a table, once with `Text` and once with the read-only `TextEdit` | same line breaks and block heights (±1 px), link taps work, no keyboard opens when tapping | whole-block selection only (author decision, research R-7) |
| S-3 | `clipboard.copy("x")` then `clipboard.text()` inside the app; note whether `QClipboard` reports a platform clipboard | round trip works | in-app buffer (always the fallback; recorded as the delivered behavior) |

Record each result in the feature's walkthrough note; they do not change the design unless a
fallback column says so.

## 3. Host scenarios (what the tests prove)

| Scenario | Spec | Where |
|----------|------|-------|
| Type "abc", paste "XY", undo → "abc"; redo → "abcXY" | US2-1..3 | `test_edit_history` |
| A 40-character sentence typed without a pause is undone in ≤ 3 steps | SC-004 | `test_edit_history` |
| Cut then paste at the same position, delete then undo restore text and caret/selection | SC-002 | `test_edit_history`, `test_edit_bridge` |
| Edit title, text, category, favorite; undo order is the reverse | US2-6, US2-9..11 | `test_edit_history` |
| After undoing everything the opened values are current again (dirty compares equal) | US2-7, SC-005 | `test_edit_history` |
| Clipboard text with CRLF, control characters, 2,000,000 characters | FR-032 | `test_text_clean` |
| Title paste with line breaks → spaces; over 255 → refused, unchanged | FR-008 | `test_edit_bridge` (`ClipboardBridge`) |
| `**bold**`, `# Heading`, `- [ ] task`, `[a](https://x.invalid)` fully selected copy their marks; two words inside bold copy none | US4-8..10, FR-031 | `test_copy_source` |
| `openNote` `hasTitle` false for an empty title; list still shows "(untitled)" | FR-024 | `test_notes_bridge` |
| `readerCopy` leaves title, text, modified time and pending state unchanged | SC-010 | `test_notes_bridge` |

## 4. Device walkthrough (InkPad One)

Prerequisite: spikes S-1..S-3 done; a disposable account or local notes: one note with a heading,
bold text, a list, a task list, a link, a table and an attachment; one note whose first line repeats
its title; one note with a 255-character title. Install per the standing instructions for the
mounted reader, then:

| # | Step | Expected |
|---|------|----------|
| 1 | Open the note in the editor. Look at the header | only Cancel, Save, Favorite, Attach (Delete for a saved note) — no command buttons (FR-003) |
| 2 | Long-press a word in the text | the word is shown selected, a black marker hangs under each end, a bubble with Cut, Copy, Remove, Select all (and Undo) appears above it; the keyboard stays (US1-1, FR-010, FR-034) |
| 3 | Choose Copy, tap at the start of the text, long-press empty space below the text, choose Paste | the word is inserted there; the bubble for empty space offers Paste, Select all, Undo only (US1-2, US1-6) |
| 4 | Long-press a word, choose Cut, then long-press empty space and choose Undo | the word is back and selected (US1-4, US2-2) |
| 5 | Long-press a word, choose Remove, then Paste | the word is gone; Paste inserts the *previous* clipboard (US1-5) |
| 6 | Long-press a word, drag the end marker over more words, lift the finger | the selection follows the marker; the bubble hides while dragging and comes back (US1-7, FR-034); drag the start marker too |
| 7 | Long-press a word in the Title, choose Select all, Copy; long-press in the text and Paste there; Copy two lines from the text and Paste into the Title | the title is inserted in the text; the pasted lines become one line with a space (US1-9, US1-10, US1-12) |
| 8 | Paste a text over 255 characters into the Title | message "The title is too long…"; title unchanged (FR-008) |
| 9 | Type a sentence, tap Undo once, again | the sentence disappears in 1–3 taps; Redo restores it (SC-004) |
| 10 | Change the category, toggle Favorite, type a word; tap Undo three times | word, favorite, category are undone in that order (US2-9..11) |
| 11 | Undo everything, tap Cancel | no "Discard changes?" prompt (US2-7, FR-017) |
| 12 | Save, reopen the editor | Undo is disabled (US2-8) |
| 13 | Rotate to landscape and back (keyboard closed first) | the long-press menu and markers keep working and the bubble stays inside the screen |
| 14 | Open a note on the reading page | its title is shown above the text in bold, larger type; "(untitled)" is never shown; an empty-title note shows no gap (US3-1, FR-024) |
| 15 | Open the note whose first line repeats its title | the title block and the repeating first line are both shown (US3-3, FR-022) |
| 16 | Open the note with a 255-character title; try A−/A+ | the title wraps in full at every size (SC-008, US3-2, US3-4) |
| 17 | Page through a long note | the title scrolls away with the content (US3-5) |
| 18 | Tap Edit | the Title field is shown, no extra heading (US3-6); save a changed title and return: the new title shows |
| 19 | On the reading page long-press a word; choose Copy; leave; open another note's editor; long-press empty space, choose Paste | the word is inserted; the first note is unchanged; the reading page's bubble only offers Copy and Select all (US4-1..4, SC-010) |
| 20 | Select a heading, a bold phrase and a task item completely (drag the markers); Copy; Paste in an editor | `# Heading`, `**bold**`, `- [ ] task` as in the source (US4-8, 4-9) |
| 21 | Select two words inside a bold phrase; Copy; Paste | the two words only (US4-10) |
| 22 | Long-press a word, drag the end marker down past the page end, turn the page with the page button, drag it further | the range spans both pages and copies as one; the bubble closes on the page turn and returns when a marker is touched (spec edge case, FR-030) |
| 23 | Tap a link, a task box, an attachment with no selection; and with a selection | no selection: as before; with a selection: the first tap only ends it (US4-7) |
| 24 | Select, change the font size | the selection ends (US4-6) |
| 25 | Long-press a table row | the row is framed; Copy returns the row as written (spec edge case) |
| 26 | Check that Copy on the reader changes neither the list order nor the "pending" marker | unchanged (SC-010) |
| 27 | **S-3 result**: copy in the app, switch to the reader's own apps and try pasting | recorded as the delivered behavior (in-app or shared) |

Record any deviation in the feature's walkthrough note (as `docs: record the device walkthrough`
commits do for earlier features).
