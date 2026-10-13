# Quickstart: validating the Built-in Look overhaul

Validation guide only; implementation details live in `tasks.md`. Contract:
[ui-components](contracts/ui-components.md). Data: [data-model](data-model.md). Decisions: [research](research.md).
Build, packaging and device prerequisites are those of `specs/001-pocketbook-notes-mvp/quickstart.md` §1–§3.

The feature ships in five slices (R-12). Sections 1 and 2 run after **every** slice; section 3 lists what each slice's
device walkthrough adds. A slice is done only when all of its rows pass (SC-010).

## 1. Host build and tests (no device)

```bash
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host
ctest --test-dir build-host --output-on-failure
scripts/pre-merge.sh
```

Expected: all suites pass offline, including every pre-existing suite unchanged in its assertions about stored data,
sync and attachments (SC-004, SC-009). New or extended, each written first and observed failing (constitution I):

| Test | Slice | Proves |
|------|-------|--------|
| `test_launcher_icon` (host, reads the BMPs) | 1 | no frame ring, glyph 60–85 % and centred, two variants differ (FR-001..FR-004) |
| `test_store_open` + `test_notes_bridge` (`list_layout`) | 2 | default `list`, accepts only `list`/`tile`, persists, bridge property signals |
| `ui_style.qml` | 2 | `Style` singleton loads; every icon in the asset list loads; a missing icon falls back to the label |
| `note_list.qml` | 2 | top bar content and upper-case title; filter/sort labels; count line; List↔Tile toggle keeps view/sort/search; tile and row taps open the note; whole-row paging in both layouts; remembered layout; empty states; sync status and pending count shown; search opens, filters, closes |
| `ui_dialogs.qml` | 3 | each dialog: heavy outline, title + close icon, two footer buttons with dismiss first, scrim swallows touches, Back closes without effect; pointer menus keep their placement tests |
| existing `editor_slash.qml`, `editor_flow.qml`, `reader_select.qml` | 3, 4 | pass with the new menus and bars (updated first where a control was a text button) |
| `reader_page.qml` (new) | 4 | top bar, edit icon, caption, text-size tool opens the slider panel, three stops, `N pt` label, page indicator, edge taps still page |
| `editor_flow.qml` (extended) | 4 | undo/redo/favorite/save in the bar, favorite toggles its icon, attach/category/delete in the panel above the keyboard, dirty-leave dialog |
| `settings_sync.qml`, `setup_page.qml`, `about_pages.qml` (extended/new), `file_picker.qml` | 5 | switch toggles Auto Sync, rows, top bars, attach browser behaviors of 009 unchanged |
| `no_boxed_buttons` (source scan) | 5 | no `InkButton`/bordered text button remains in `qml/` (SC-003) |
| `app_self_test` | all | every page instantiates, a page is shown, no QML warnings |

## 2. Host look check (while tuning, optional but expected)

```bash
PBNOTES_QML_SHOT=/tmp/list.png PBNOTES_SHOT_W=1404 PBNOTES_SHOT_H=1872 PBNOTES_START_PAGE=pages/NoteListPage.qml \
    build-host/app/pbnotes_app
```

Compare the picture with `screens/scr0002` (list), `scr0005` (dialog), `scr0004` (menu) and `scr0010` (list rows) from the
reader. The host renders grey differently from the panel, so this is for layout and proportions only.

## 3. Device walkthrough (InkPad One)

Install the slice's build (`scripts/package-device.sh`, then copy as in earlier features). Prerequisite: an account with a
mix of notes (a favorite, a note without category, a very long title, a note with an attachment, ≥ 40 notes for paging).
Take the device's own screenshot (the reader's screenshot function writes `screens/scr….bmp`) after each numbered step
marked 📷 and compare it with the reference named in the last column.

### Slice 1 — icon (1.6.0)

| # | Do | Expect | Reference |
|---|----|--------|-----------|
| 1.1 | Open the application menu 📷 | PB Notes has no square frame; it is line art of the same weight and size as *Notizblock*, *Rechner*, *Wörterbuch* | scr0001 |
| 1.2 | Tap and hold / select the icon so the pressed state shows | Frameless inverted variant, distinguishable from the normal one | scr0001 |
| 1.3 | Compare at arm's length with the neighbours | Not the odd one out in a blind check against three neighbours (SC-001) | scr0001 |

### Slice 2 — note list (1.7.0)

| # | Do | Expect | Reference |
|---|----|--------|-----------|
| 2.1 | Start the app 📷 | Top bar (home, menu, `NOTES`, search, new) below the system status bar if one is shown; filter/sort row; count line; rows with bold title and quiet category/date line; hairlines | scr0002 |
| 2.2 | Read the log for `font.ui` | Roboto present (`roboto=1`) or the bundled font used; typography matches the screenshot (R-5 gate) | scr0002 |
| 2.3 | Tap the view label | A bubble with a pointer lists All, Favorites, Uncategorized and each category with its count; the current one is marked; choosing filters the list | scr0004 |
| 2.4 | Tap the sort label, search icon, then type | Sort choice changes the order and the label; search opens a field with a close control; the list filters; closing clears | scr0002 |
| 2.5 | Tap the layout toggle, leave the app, restart | Tiles (2 per row portrait, 3 landscape) with title, category/date, star; same notes and order; restart opens in tiles | scr0002 (grid icon) |
| 2.6 | Open the menu icon | *Sync now* and *Settings* are there and work; the sync status line and `n change(s) not synced yet` show | — |
| 2.7 | Page through ≥ 40 notes in both layouts | Whole rows/cards per page, no cut row, chevrons enable/disable correctly; at least as many notes per page as 1.5.0 (SC-007) | — |
| 2.8 | Rotate to landscape | Top bar, rows and tiles lay out without overlap | — |
| 2.9 | Note the status-bar finding | If the system strip overlays the bar, set `Style.topInset` (R-9) and repeat 2.1 | scr0002 |
| 2.10 | Create a note, find it by search, open it (SC-005) | No more taps than in 1.5.0 | — |

### Slice 3 — dialogs and menus (1.8.0)

| # | Do | Expect | Reference |
|---|----|--------|-----------|
| 3.1 | In the editor: delete a note's text, tap back | "Discard changes?" dialog: heavy rounded outline, bold title + close icon over a rule, page dimmed, footer `KEEP EDITING | DISCARD` split by a rule 📷 | scr0005 |
| 3.2 | New category (text entry) with the keyboard | Dialog sits above the keyboard; field has a label and thin rounded outline; error line shows for a bad name | scr0005 |
| 3.3 | Long-press a word | Rounded pop-up with a pointer to the word, icon-only entries | scr0007 |
| 3.4 | Type `/` | Pop-up with icon + label rows and dotted dividers; date/time and emoji pickers open as dialogs | scr0004 |
| 3.5 | Press the device's back key with each dialog and menu open | Each closes without changing anything | — |

### Slice 4 — reader and editor (1.9.0)

| # | Do | Expect | Reference |
|---|----|--------|-----------|
| 4.1 | Open a note 📷 | Top bar (back, title, pencil); caption; bottom panel *Text size*; `n / total`; edge taps and chevrons still page | scr0008 |
| 4.2 | Tap *Text size* 📷 | Panel with round minus/plus, ring-knob slider with 3 stops, `N pt`; the text re-flows at once | scr0009 |
| 4.3 | Tap the pencil | Editor: top bar (back, title, undo, redo, star, save); title field; panel *Attach | Category | Delete* above the keyboard | scr0003 |
| 4.4 | Toggle the star; type; undo/redo; save | Star icon fills/unfills; undo/redo/save work; leaving with changes asks first | — |
| 4.5 | Attach a file, set a category, delete a note | Each works from the panel with the keyboard up and down, no extra tap vs 1.8.0 (FR-023) | — |
| 4.6 | Landscape with the keyboard up | Nothing clipped or overlapping | — |

### Slice 5 — remaining screens (1.10.0)

| # | Do | Expect | Reference |
|---|----|--------|-----------|
| 5.1 | Open Settings | Top bar `SETTINGS`; hairline rows; Auto Sync is a pill switch showing `on`/`off`; rotation choice marked | scr0009 |
| 5.2 | Open About, licenses, third-party | Top bar with back; rows; text readable; paging works | scr0010 |
| 5.3 | Sign out/in (setup page) with the keyboard | Fields with labels and thin outlines; connect works as before | scr0005 |
| 5.4 | Attach browser, both layouts, previews | Same behavior as 009; *Up* is a chevron in the bar; toggle in the bar | scr0002 |
| 5.5 | Walk every screen | No bordered text button anywhere (SC-003); every earlier action works (SC-004); every screen in all four orientations (SC-006) | — |

Record the measured values (notes per page, tiles per row, the font and inset findings, the pt labels) in
`checklists/device-results.md`; adjust `Style.qml` if a row fails.
