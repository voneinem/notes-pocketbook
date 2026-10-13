# Quickstart: validating the Attach File Browser

Validation guide only; implementation details live in `tasks.md`. Contract:
[qml-bridge](contracts/qml-bridge.md). Data: [data-model](data-model.md). Decisions:
[research](research.md). Build, packaging and device prerequisites are those of
`specs/001-pocketbook-notes-mvp/quickstart.md` §1–§3.

## 1. Host build and tests (no device)

```bash
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host
ctest --test-dir build-host --output-on-failure
scripts/pre-merge.sh
```

Expected: all suites pass offline. New: `test_file_preview_provider`, `file_picker.qml`. Extended:
`test_attachments_bridge`, `test_store_open`, `stubs.qml`. Each is written first and observed failing
before its implementation (constitution I). The existing attachment tests (staging, size limit, upload,
discard) must pass unchanged (SC-008). `app_self_test` must still pass.

## 2. Host scenarios (what the tests prove)

| Scenario | Spec | Where |
|----------|------|-------|
| root lists only the seven folders; Autorun.inf, install.md and other folders are absent; names match ignoring case; a file with a folder's name is absent; missing folders are absent; empty list when none exist | US1-1, 1-4, 1-5, FR-001..FR-003 | `test_attachments_bridge` |
| inside a named folder every subfolder and file is listed, in the same order as before | US1-2, FR-004, FR-013 | `test_attachments_bridge` |
| `image` / `previewId` only for regular readable image files | FR-017, FR-018 | `test_attachments_bridge` |
| `picker_view` defaults to `list`, accepts `list`/`tile` only, persists, survives forget account | FR-012 | `test_store_open`, `test_attachments_bridge` |
| provider: scales, never enlarges, grayscale, keeps proportions; refuses unknown/outdated ids, corrupt files, > 40 MP, non-regular files | US3-1, 3-4, FR-017, FR-019 | `test_file_preview_provider` |
| provider: cache stays under 8 MB over hundreds of requests; a replaced file is not served from the old entry; a cancelled queued request does not decode | US3-5, FR-020, FR-021, SC-006 | `test_file_preview_provider` |
| the original file is byte-identical and untouched after previews; attaching copies the original | US3-6, FR-022, SC-007 | `test_file_preview_provider`, `test_attachments_bridge` |
| Up disabled at the root, returns from a first-level folder to the root; start at the root; toggle keeps folder; start in the stored view; folder and file taps in both views; oversized file message; whole-row paging; empty root message; two quick taps enter one folder | US1-3, 1-6, US2-1..2-7, FR-005..FR-016 | `file_picker.qml` |

## 3. Device walkthrough (InkPad One)

Prerequisite: a reader whose storage root holds the seven folders by name plus `Autorun.inf`, `install.md`
and at least one other folder; "Photo" and "screens" with a mix of at least 40 images (some large), a PDF
and a subfolder; one damaged `.jpg`. Install the build as for earlier features.

| # | Do | Expect |
|---|----|--------|
| 1 | Edit a note, tap Attach | Exactly the seven folders (those that exist) in List view (first use); no Autorun.inf, install.md or other folder; Up is greyed |
| 2 | Open "Photo", then Up, Up | Photo's whole content is listed; the first Up returns to the seven folders; Up is greyed there |
| 3 | In "Photo" tap the toggle | The same folder as tiles, more entries on the page (about twice as many as the list); the icon now shows three bars |
| 4 | Watch the page after the toggle | Tiles are tappable at once; image previews appear within about 10 s, grayscale and recognizable; the PDF and the subfolder show glyphs |
| 5 | Tap Page down / Page up several times, quickly | The grid moves by whole rows; the page stays responsive while previews load; previews of the left page are not slowing the new one |
| 6 | Tap the damaged `.jpg` tile | It shows the file glyph; tapping it still asks "Attach this file?" with name and size |
| 7 | Pick a large photo and confirm | The original is attached (check size in the note); the file on the card is unchanged |
| 8 | Toggle to list, back out of the browser, restart the app, open Attach | The browser starts at the seven folders in the last chosen view |
| 9 | Rotate the device while the browser shows tiles | The grid re-flows to more columns in landscape; folder and view stay |
| 10 | Browse "Photo" and "screens" back and forth for five minutes, toggling now and then | No slowdown, no crash; revisiting a page shows its previews quickly |
| 11 | Rename "Photo" to "photo" on the card (via USB), then reopen the browser | It still appears |
| 12 | Rename "My pictures" to something else, reopen the browser | It disappears from the list; nothing else changes |

Record the real values found in step 3 and 4 (tiles per page, time to previews) in
`checklists/device-results.md`; adjust the tile size in `FilePickerPage.qml` if SC-003 or SC-005 is not met.
