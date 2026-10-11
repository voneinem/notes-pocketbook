# Device and host walkthrough: feature 005 (editing tools + reader title)

**Date**: 2026-10-11 · **Build**: pbnotes 1.1.0 (branch `feature/005-text-editing-reader-title`, 1.2.0 not yet released)

Rows refer to `quickstart.md` §2 (spikes) and §4 (device walkthrough). Host rows were run by the implementing agent;
every [MANUAL] task is still open until the reader results are recorded here.

## Host results

| Task | Check | Result |
|------|-------|--------|
| T011 | Editor page rendered with the desktop app (offscreen; the screenshot tool ignores the requested size and renders 800×800) | The tools row (All, Extend, Copy, Cut, Paste, Del; Undo/Redo hidden until US2) ends at x = 673 of 800, so with Undo and Redo (~220 px more) it stays under the 1020 px content width of the 1080 px portrait page. Copy/Cut/Del/Extend are greyed without a selection, Paste is greyed with an empty clipboard. No label is elided. |

| T025 | Reading page driven offscreen with stub data (`tests/qml/reader_title.qml`, CTest `qml_reader_title`) | A short, a 255-character and an empty title: the title block is shown with the stored title, a 255-character title wraps onto several lines (height > 3 lines) and is passed on whole, the block follows `fontPx`, an empty title leaves no block and no gap, a first line that repeats the title is still shown below the title block. (No picture: offscreen `grabToImage` does not complete.) |
| T019 | `tests/qml/editor_flow.qml` (CTest `qml_editor_flow`) drives the real `EditorPage.qml` | select, Copy, Cut, Paste, Delete, Extend, title paste rules and Undo/Redo over all four fields: 36 checks pass |

| T031 (host half) | `Text` (StyledText) vs read-only `TextEdit` (RichText in a `white-space: pre-wrap` span) probed offscreen on eight sample texts (double spaces, leading/trailing space, bold/italic/link, entities, emoji, a wrapping line) | Identical content heights in every case; with the pre-wrap span the document text equals the source text exactly (without it spaces collapse, which would misalign offsets), so the span is part of `SelectableText.qml`. The device half of S-2 is still open. |
| T039/T040 | `tests/qml/reader_select.qml` (CTest `qml_reader_select`) drives the real `ReaderPage.qml` | word hold, Copy (range passed to the bridge), Extend forwards and backwards across units, tap inside/outside, All, whole units (blank line, table row), the title unit, links not followed while a selection exists, font-size change and reload end the selection, footer (All, Extend, Copy, Page up/down) fits 1080 px |
| Build | `scripts/pre-merge.sh`; `cmake --build build-arm` | all 82 host tests pass; ARM binary builds and links; `dist/pbnotes-inkpad-one-1.2.0.zip` packaged and `pbnotes.bin`/`pbnotes.app` installed on the reader (cmp-verified) |

## Device feedback (2026-10-11, the author, informal)

- First build (tools row, `TapHandler`): too many buttons in the editor; selecting in the editor did not work at all.
  Led to the rework of Phase 8 (long-press menu, draggable markers, `MouseArea` gestures).
- Reworked build: "much better". One observation: after Select all a tap elsewhere did not end the selection
  (a tap in another field or on empty space). Fixed (editor and reading page) and re-installed; the author confirmed:
  "Perfect!". Row-by-row results of quickstart §4 were not recorded.

## Spikes (open)

| # | Check | Result |
|---|-------|--------|
| S-1 | press-and-hold selects a word, keyboard stays | works with `MouseArea.pressAndHold` (device feedback above); the `TapHandler` variant is gone |
| S-2 | read-only `TextEdit` renders like `Text` | open (T031) |
| S-3 | clipboard round trip in the app; platform clipboard? | open (T007) |

## Device walkthrough (open)

Rows 1–27 of `quickstart.md` §4: open (T012, T020, T026, T041, T045).
