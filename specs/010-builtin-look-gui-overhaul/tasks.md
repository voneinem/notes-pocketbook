---

description: "Task list for Built-in Look (GUI Overhaul to Match the Reader's Own Apps)"
---

# Tasks: Built-in Look (GUI Overhaul)

**Input**: Design documents from `/specs/010-builtin-look-gui-overhaul/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/ui-components.md, quickstart.md

**Tests**: REQUIRED. The constitution makes Test-First non-negotiable (Principle I): every test task below MUST
be written and observed FAILING before the implementation task(s) it names (a test that does not load a missing
component, or does not compile against a missing member, counts as failing). Unit tests under `tests/unit/` are
picked up by the glob in `tests/CMakeLists.txt`; bridge tests are listed by name in the bridge loop of
`tests/CMakeLists.txt`; a QML page test needs an `add_test(... test_qml_pages <driver.qml> <page.qml>)` entry in the
QML block of the same file, with `ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "bridge"` like its neighbours.

**Organization**: Tasks are grouped by user story, and the five stories are the five **slices** of the plan (R-12): each
story phase ends with its own version bump and device check, so the app is releasable after each phase (FR-024).
Paths are relative to the repository root. This feature builds on features 001–009; all existing tests must stay
green after every task that touches shared code. The core library is touched only for one setting (US2).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US5 as in spec.md
- **[MANUAL]**: needs a person or the reader; record the result in the named file. An implementing agent skips these
  and reports them as open
- **[GATE]**: a [MANUAL] result that decides whether the tasks marked *only if* run

## Conventions that apply to every task

- C++17 only where a task says so (`bridge/`, `app/`, one setting in `core/`); `core/` stays Qt-free
  (`scripts/check-constitution.sh` R3). Everything else is QML (Qt Quick 6.8) or a Python 3 stdlib script.
- **No names in logs**: the one new log line (`font.ui`) carries a font family and a 0/1 flag only; log keys must not
  be, start or end with `url`, `path`, `file`, `name` or `address` (R6): use `family`, `roboto`.
- **Numbers from the design** (apply in code, not by convention; all live in `qml/Style.qml` once it exists, and
  nothing else states them):
  - Ink `#000000`; hairline `#808080`, 1 px; rule 2 px; heavy outline 4 px; pressed fill `#d8d8d8`; disabled ink `#9a9a9a`;
    scrim = white at opacity 0.6 (the page behind reads grey).
  - Radii: field 10, dialog and bubble 18. Touch minimum `touchMin` 96 (hit area of every icon control).
  - Top bar `barHeight` 112 (content below `topInset`, default 0); bottom bar `bottomBarHeight` 92; filter/sort row
    100; count line 60; list row 132 (as today); tile cell height 190, `columns = max(2, floor(width / 420))`.
  - Icon sizes: top bar 64, tool 56; icon grid 96 × 96, stroke 4, round caps and joins.
  - Type: title 44 (upper-case), row title 40 bold, body 32, label 30, caption 28, footer button 38.
  - Text-size label: `ptFor(px) = round(px × 72 / 300)` → 30 px = 7 pt, 38 px = 9 pt, 50 px = 12 pt.
  - Slider: track hairline with 12 px end dots, ring knob 76 px outer (6 px ring, white centre), round buttons 80 px.
  - Reader: the visible text area at 1404 × 1872 MUST be at least as high as the 1.5.0 value recorded in T002
    (SC-007), so the reader's bottom bar is 92 and its top bar 112.
- **Every new control has `text` and `clicked`** (contract §3), so the driver scripts under `tests/qml/` that find
  controls by `text` keep working. A control that is an icon uses its label as `text`.
- UI strings go through `qsTr`; all labels are English (the app has no translations); upper-case is made with
  `font.capitalization: Font.AllUppercase`, never by changing the string.
- Interfaces are those of `contracts/ui-components.md`; if a task finds the contract wrong, fix the contract in the
  same commit.
- Every new QML file in `qml/` is added in the same commit to **three** places: `qml/qml.qrc`, `qml/qmldir` (once it
  exists, T008) and, for a new icon, `qml/icons/NAME.svg` plus a `<file>icons/NAME.svg</file>` line in `qml/qml.qrc`.
- Icon SVGs: `viewBox="0 0 96 96"`, one or two `<path>`/shape elements, `stroke="#000000" stroke-width="4"
  stroke-linecap="round" stroke-linejoin="round" fill="none"`; a `-filled` variant uses `fill="#000000"`; no text, no
  gradients, no external references, transparent background.
- No bordered text button may be *added*; no new literal color, line width or pixel size of the look may be written in
  a page or component once `Style.qml` exists (read `Style.x`).
- Test fixtures are generated in the test; nothing real, no binary fixture is committed.

---

## Phase 1: Setup

- [X] T001 Build the host target and run the whole suite on the untouched branch (`cmake -S . -B build-host -G Ninja
  -DCMAKE_BUILD_TYPE=Debug && cmake --build build-host && ctest --test-dir build-host --output-on-failure`). Every
  test must pass (including `app_self_test` and every `qml_*` test); if one does not, stop and report it — later
  failures must be attributable to this feature.
- [X] T002 [P] Record the 1.5.0 baselines the success criteria compare against, in
  `specs/010-builtin-look-gui-overhaul/checklists/baseline.md` (new file). Measure with a throwaway driver kept in the
  scratchpad (not committed), run through `build-host/tests/test_qml_pages` at a 1404 × 1872 window: (a) the note list
  page `qml/pages/NoteListPage.qml` with 60 stub notes: `floor(list.height / 132)` where `list` is its `ListView`
  (SC-007 list); (b) `qml/pages/ReaderPage.qml`: the height of the item with `objectName: "flick"` (SC-007 reader);
  (c) the bounding box of the dark pixels of `res/icons/pbnotes.bmp` (the framed icon, for the record). Write the three
  numbers and how they were measured; later tasks quote them.

---

## Phase 2: User Story 1 — The app's icon fits in the application menu (Priority: P1) — slice 1, 1.6.0

**Goal**: the PB Notes launcher icon has no frame and sits with the built-in icons.

**Independent Test**: install the build and open the application menu: no square frame, line art of similar weight and
size as *Notizblock* and *Rechner*; the pressed variant is frameless too (quickstart 1.1–1.3).

- [X] T003 [US1] Write `tests/unit/test_launcher_icon.cpp` first (plain C++, no Qt; uses `PBNOTES_SOURCE_DIR`, the
  `PB_TEST`/`PB_CHECK` macros of `tests/support`; see another file in `tests/unit/` for the include). It parses
  `res/icons/pbnotes.bmp` and `res/icons/pbnotes_f.bmp` (BITMAPINFOHEADER, 8 bpp, bottom-up rows padded to 4 bytes,
  grey palette where index = grey level) and checks, per variant: width and height are 122; **the outer 10 px ring is
  entirely the background value** (255 in the normal variant, 0 in the focused one: this is "no frame", FR-001/FR-003);
  the bounding box of pixels differing from the background by more than 64 is between 60 % and 85 % of 122 in both
  directions (73 to 103 px) and its centre is within 4 px of the canvas centre (FR-002); at least 100 pixels have a
  value strictly between 40 and 215 (anti-aliased strokes); and across the two variants at least half of all pixels
  differ (FR-003). It MUST fail on the current framed icon (observe the failure, quote it in the commit message).
- [X] T004 [US1] Rewrite the drawing in `scripts/make-icons.sh` (keep the pure-Python, stdlib-only form and the two output
  files): draw at 4× (488 × 488) and box-filter down to 122 × 122 8-bit grey so strokes are anti-aliased; **no outer
  frame**; the sheet outline is 3 px wide, x from 23 to 99 and y from 15 to 107 (76 × 92 px), corners rounded with
  radius 12; a solid header strip 9 px high inside the top of the sheet; three full text lines (x from 34 to 88) and a
  short fourth line (x from 34 to 70), each 4 px high, at y = 42, 56, 70, 84; the normal variant black on white, the
  focused variant the inverse (`255 - v`). Run `scripts/make-icons.sh`, commit the regenerated `res/icons/pbnotes.bmp`
  and `res/icons/pbnotes_f.bmp`. T003 must now pass. Open both BMPs once to confirm they look like the glyph and compare
  their weight to a neighbour icon in `screens/scr0001.bmp` of the reader; adjust the numbers above only if the test's
  limits still hold.
- [X] T005 [US1] Version: set `VERSION` to `1.6.0` and add a `## 1.6.0` entry at the top of `CHANGELOG.md`: the launcher
  icon no longer sits inside a square frame and matches the built-in icons in size and weight; nothing else changes.
  `scripts/check-version-bump.sh` must pass.
- [ ] T006 [MANUAL] [US1] Package (`scripts/package-device.sh`) and install per `INSTALL.md`; run quickstart §3 slice 1
  (rows 1.1–1.3), take the screenshot of the application menu, record the result and the screenshot name in
  `specs/010-builtin-look-gui-overhaul/checklists/device-results.md` (new file). If the glyph is visibly heavier or
  larger than its neighbours, adjust the numbers in T004 and repeat T004–T006.

**Checkpoint**: after T006 the icon change can be merged and tagged `v1.6.0` on its own.

---

## Phase 3: User Story 2 — The note list looks and behaves like the built-in Notes list (Priority: P1) — slice 2, 1.7.0

**Goal**: the shared foundation (`Style`, icons, `IconButton`, `TopBar`), then the note list on top of it, with the Tile
layout and its remembered setting.

**Independent Test**: open the note list with a mix of notes; its structure matches `screens/scr0002` (top bar, filter/
sort row, count line, rows with hairlines); view, sort, search, create, sync and Settings all work in at most two taps;
the Tile layout shows the same notes and is remembered across a restart (quickstart 2.1–2.10).

### Foundation (the later stories depend on it)

- [X] T007 [US2] Write `tests/qml/ui_style.qml` first and register it as `qml_ui_style` in `tests/CMakeLists.txt`
  (page argument: `${CMAKE_SOURCE_DIR}/qml/Icon.qml`; the driver imports the `qml/` directory by relative path
  `import "../../qml"` so the singleton resolves through `qml/qmldir`). Checks: `Style.ruleWidth === 2`,
  `Style.heavyWidth === 4`, `Style.barHeight === 112`, `Style.touchMin === 96`, `Style.topInset === 0`,
  `Style.ptFor(30) === 7`, `Style.ptFor(38) === 9`, `Style.ptFor(50) === 12`; **every icon name of the slice-2 set
  loads** (`Icon { source: name }` reaches `Image.Ready`, none `Image.Error`) for the names listed in T009; an `Icon`
  with an unknown name reports `ready === false`; an `IconButton` with `text: "Search"`, `icon: "search"` has `text`
  "Search", a hit area of at least 96 × 96, and with `toggled: true` shows `star-filled` when its icon is `star`;
  an `IconButton` tap emits `clicked` once and a disabled one does not; a `TopBar` with `title: "Notes"` shows "Notes"
  upper-cased (`font.capitalization === Font.AllUppercase`) centred horizontally within 2 px and a 1 px hairline at its
  bottom edge, and its content starts at `Style.topInset`; a `RowDivider` is as wide as its parent. MUST fail now
  (no files).
- [X] T008 [US2] Create `qml/qmldir` (no `module` line: it is used as an implicit directory import by `import ".."`) with
  `singleton Style 1.0 Style.qml` and one line `TypeName 1.0 File.qml` for **every** QML file already in `qml/`
  (`InkButton`, `PageButtons`, `BlockText`, `TitleBlock`, `SelectableText`, `TableRowView`, `AttachmentView`,
  `ConfirmDialog`, `ChoiceDialog`, `TextEntryDialog`, `SelectionHandle`, `SelectionHandles`, `SelectionMenu`,
  `SlashMenu`, `DateTimePicker`, `EmojiPicker`; not `Main.qml`) because a directory with a `qmldir` exposes only the
  listed types. Create `qml/Style.qml` (`pragma Singleton`, an `QtObject` with the constants of the *Numbers from the
  design* above and `function ptFor(px)`). Add `qml/qmldir` and `qml/Style.qml` to `qml/qml.qrc`, and add
  `${CMAKE_SOURCE_DIR}/qml/qmldir` and `${CMAKE_SOURCE_DIR}/qml/icons/*.svg` to the dependency globs in
  `app/CMakeLists.txt` (`PBNOTES_QML_FILES`) so the manual ARM `rcc` rebuilds when they change. Run the **whole**
  suite: every existing page and driver must still load (this is the spike of R-2; if a type is not found, a line is
  missing in `qml/qmldir`). The Style-related checks of T007 pass.
- [X] T009 [P] [US2] Create the slice-2 icons in `qml/icons/` and list each in `qml/qml.qrc` as
  `<file>icons/NAME.svg</file>`: `home`, `menu` (three horizontal lines), `search`, `note-plus` (a page with a folded corner
  and a plus), `chevron-left`, `chevron-up`, `chevron-down`, `close`, `grid` (2 × 2 rounded squares), `list` (three
  bars), `filter` (three decreasing lines), `star`, `star-filled`, `gear`, `sync` (two arrows in a circle) and
  `row-dots.svg` (a 6 × 2 px tile with a single 2 × 1 px black rect, used with `fillMode: Image.Tile` for dotted
  dividers). Draw them in the style of the built-in icons in `screens/scr0001`/`scr0002`/`scr0010` (outline, 4 px
  stroke), original drawings only.
- [X] T010 [US2] Create `qml/Icon.qml` (properties `source` — an icon name, `size`; an `Image` with
  `source: "qrc:/qml/icons/" + source + ".svg"`, `sourceSize: Qt.size(size, size)`, `smooth: true`, `cache: true`;
  `readonly property bool ready: status === Image.Ready`) and add it to `qml/qmldir` and `qml/qml.qrc`. T007's icon
  checks pass.
- [X] T011 [P] [US2] Create `qml/IconButton.qml`, `qml/TopBar.qml`, `qml/RowDivider.qml` per contract §3 (add each to
  `qml/qmldir` and `qml/qml.qrc`): `IconButton` — `text`, `icon`, `toggled`, `enabled`, `clicked()`; an `Item` of at
  least `Style.touchMin` square with an `Icon` (size 64) centred, a light-grey fill while pressed, `Style.disabled`
  tint when disabled (use `opacity: 0.4`), and, when the `Icon` is not `ready`, a centred `Text` of its `text` (the
  fallback); no focus on tap (`focusPolicy`-like: use a `MouseArea`). `TopBar` — an `Item` of height
  `Style.barHeight`, `y: Style.topInset`, `leftItems`/`rightItems` default-slot `Row`s, the title a centred upper-case
  `Text` elided to the room between the slots, a 1 px hairline at the bottom. `RowDivider` — an `Image` of
  `row-dots` with `fillMode: Image.Tile`, 2 px high. T007 passes completely.
- [X] T012 [P] [US2] UI typeface (R-5). In `app/main.cpp`, where the emoji fallback is set up, choose the family: when
  `QFontDatabase::families()` contains "Roboto", use it as the first entry of the application font's families
  (before the emoji family), otherwise keep the current default; log one event `font.ui` with `family` (the chosen
  family name) and `roboto` (1 or 0) right after the existing `image.formats` log line. No other behavior changes. No
  unit test exists for start-up wiring (same as the emoji font); the line is checked on the device in T025 and the
  existing `app_self_test` must still pass.

### Setting and bridge: the remembered layout

- [X] T013 [P] [US2] Write the tests first: in `tests/unit/test_store_open.cpp` add `list_layout_is_list_by_default_and_
  accepts_only_list_and_tile` (copy the shape of `picker_view_is_list_by_default_and_accepts_only_list_and_tile`:
  default `list`; accepts `tile` and `list`; refuses `"grid"`, `""`, `"TILE"`, `"1"`, `"tiles"` keeping the stored
  value; survives reopening); in `tests/unit/test_account_store.cpp` extend the test around line 97 so `list_layout`
  also survives forgetting the account like `picker_view`; in `tests/unit/test_store_migration_v3.cpp` extend the
  check near line 124 so a migrated database reads `list_layout` = `list`. They fail until T014.
- [X] T014 [US2] In `core/src/note_store.cpp` add `{"list_layout", "list"}` to `setting_defaults()` and
  `if (key == "list_layout") return value == "list" || value == "tile";` to `valid_setting()`; document the key in
  `core/include/pbnotes/note_store.h` next to `picker_view`. T013 passes; the rest of the unit suite is unchanged.
- [X] T015 [P] [US2] Write the bridge test first in `tests/bridge/test_notes_bridge.cpp`: `the_list_layout_is_list_
  on_a_fresh_install_and_remembers_a_change` (copy the shape of the `pickerView` test in
  `tests/bridge/test_attachments_bridge.cpp` around line 326): property `listLayout` reads `list`; setting `tile`
  works and emits `listLayoutChanged` once; the same value again emits nothing; `"grid"` is ignored; it persists after
  re-creating the bridge on the same database. Fails until T016.
- [X] T016 [US2] In `bridge/include/notes_bridge.h` add `Q_PROPERTY(QString listLayout READ listLayout WRITE
  setListLayout NOTIFY listLayoutChanged)` with the member functions and signal, and in `bridge/src/notes_bridge.cpp`
  implement them exactly like `AttachmentsBridge::pickerView` (read `list_layout`; `"tile"` or else `"list"`; the setter
  ignores other values, the same value and a failed store write, otherwise stores and emits). T015 passes.

### The note list

- [X] T017 [P] [US2] Extend `tests/qml/stubs.qml` for `NoteListPage.qml`: in `bridge` add `listNotes()` returning a
  fixture of 60 notes `{localId, title, category, favorite, pending, modified}` (every 7th a favorite, every 5th with
  category "Work", every 11th with category "Home", every 13th pending, one with a 120-character title, one with an
  emoji-only title, the rest plain; `modified` descending), `categories` (`[{name:"Work",count:12},{name:"Home",count:6}]`),
  `view` ("all") with `setView(v)`, `sortOrder` ("modified") with `setSortOrder(o)`, `searchText` with `setSearch(t)`,
  `syncActive`, `syncRunning` (false), `syncOutcome` ("ok"), `syncStatus` ("Synced."), `pendingCount` (3), `syncNow()`
  (counts calls), `formatTime(t)` ("Today"), `listLayout` ("list", with a write counter like `pickerViewWrites`), and a
  signal `notesChanged`; in `device` add `quit()` (counts calls). Keep the existing members.
- [X] T018 [US2] Write `tests/qml/note_list.qml` first and register it as `qml_note_list` (page argument
  `qml/pages/NoteListPage.qml`; window 1404 × 1872 for the paging checks). It MUST fail against the current page.
  Checks (each its own `check`/`eq`): the top bar exists and its title is "Notes" (upper-cased by capitalization),
  centred; controls `Home`, `Menu`, `Search`, `New note` are present as icon controls and **no control with a border
  and a text label** exists (walk the tree: no `InkButton`); the filter/sort row shows the view label "All" and the
  sort label "Recently changed" ("A–Z" when `sortOrder` is "title") each followed by a chevron, and the layout toggle; the count line reads
  "60 NOTES" — "1 NOTE" for one, "0 NOTES" for none (the text is built in JS, the capitalization does the case) — and
  carries the sync status "Synced." and "3 change(s) not synced yet"; rows show a bold title, a category/date line, a
  star for favorites and "not synced yet" for pending; tapping a row pushes `ReaderPage` with that `noteId`; tapping
  `New note` pushes the editor with `noteId: -1`; `Home` calls `device.quit()`; `Menu` opens a choice with *Sync now*
  and *Settings*, *Sync now* calls `bridge.syncNow()` once, *Settings* pushes `SettingsPage`; the view label opens the
  choice list (All, Favorites, Uncategorized, "Work (12)", "Home (6)") and choosing *Favorites* calls
  `bridge.setView("favorites")`; the sort label toggles `setSortOrder("title")`; `Search` opens a text field and a
  close control, typing calls `setSearch` after the 500 ms debounce and closing clears with `setSearch("")`; the layout
  toggle writes `listLayout = "tile"` once and shows a grid of cards (`columns` 2 at 1080 px width and 3 at 1404 px
  with `max(2, floor(width / 420))`) containing the same notes in the same order, each card with title, category/
  date, star and pending marker; a tap on a card opens the same reader; the toggle's icon changes (grid ↔ list); the
  page starts in tile when `listLayout` is `"tile"`; paging: with 1404 × 1872 `floor(listHeight / 132)` rows are
  whole on each page (no cut row after *Page down*), the first page holds at least the number of rows recorded in
  `checklists/baseline.md` (SC-007), and a tile page holds at least as many notes as a list page; empty states: no
  notes shows one of the existing messages ("No notes yet. Tap Sync now to load your notes." when `syncOutcome` is
  "never", "No notes match. Change the view or clear the search." with a filter) and the count line reads "0 NOTES";
  the `Page up`/`Page down` controls are present by `text`, disabled at the ends.
- [X] T019 [US2] Restyle the top of `qml/pages/NoteListPage.qml` (top bar and search): replace the header `Column` and
  its `InkButton`s by a `TopBar` with `leftItems` = `IconButton { text: qsTr("Home"); icon: "home" }` (calls
  `device.quit()`, replacing *Quit*) and `IconButton { text: qsTr("Menu"); icon: "menu" }` (opens the existing
  `ChoiceDialog` with the two choices *Sync now* → `syncWithWifi()` and *Settings* → push `SettingsPage.qml`),
  `title: qsTr("Notes")`, `rightItems` = `IconButton { text: qsTr("Search"); icon: "search" }` (toggles
  `searchOpen`) and `IconButton { text: qsTr("New note"); icon: "note-plus" }` (pushes the editor with `noteId: -1`);
  while `searchOpen` a row under the bar holds the existing `TextField` (same debounce `Timer`, same `onAccepted`) and
  an `IconButton { text: qsTr("Close search"); icon: "close" }` that clears the text, calls `bridge.setSearch("")`,
  hides the keyboard and closes the row. Keep `syncWithWifi()`, `connectingWifi`, `wifiMessage`, the `Connections`
  and the start-up sync exactly as they are. T018's top-bar, menu and search checks pass.
- [X] T020 [US2] Restyle the filter/sort row and the count line in `qml/pages/NoteListPage.qml`: a 100 px row with a
  `filter` icon, the view label (`viewLabel(bridge.view)`) and a `chevron-down` icon (tap opens the existing view
  `ChoiceDialog` as today), the sort label ("Recently changed" for `modified`, "A–Z" for `title`) with a chevron (tap
  flips the order as today), and at the right end the layout toggle `IconButton` (`text: qsTr("Layout")`, `icon` =
  `grid` while the list is shown and `list` while tiles are shown) that sets `bridge.listLayout`; under it a 60 px
  count line: left the count in capitals ("N NOTES"/"1 NOTE"), right-aligned and elided the sync status (the
  `wifiMessage` or `bridge.syncStatus`) plus the "n change(s) not synced yet" part when `bridge.pendingCount > 0`
  (one line, caption size). Hairlines (`Style.hairline`) under the bar, the row and the count line.
- [X] T021 [US2] Restyle the list rows in `qml/pages/NoteListPage.qml` (height stays `Style.rowHeight` = 132): the title
  in `Style.rowTitleSize` bold on one line with a `star-filled` `Icon` (size 36) before it for a favorite (replaces
  the `★` text), the second line in caption size: category, a bullet "•", the date from `bridge.formatTime`, and
  "• not synced yet" when pending; a `RowDivider` at the bottom of each row instead of the solid 1 px line; the
  pressed fill stays. Remove the `*Quit*` button and the old footer `Row`; the footer is `PageButtons` alone, centred
  (T023). T018's row checks pass.
- [X] T022 [US2] Add the Tile layout to `qml/pages/NoteListPage.qml`: a `GridView` (`objectName: "noteGrid"`) next to the
  `ListView` (`objectName: "noteList"`), both anchored in the same room between the count line and the footer, only
  one visible and `interactive: false`; `visible`/`model` follow `bridge.listLayout`; `columns = Math.max(2,
  Math.floor(width / 420))`, `cellWidth = width / columns`, `cellHeight = Style.tileHeight` (190); the card delegate is
  a 2 px `Style.hairline`-bordered rounded rectangle (radius 10) inset by 8 px with the title (bold, up to two lines,
  elided), the category/date line (one line, elided), a `star-filled` icon top-right for a favorite and the "not
  synced yet" caption when pending; a tap opens the reader exactly like a row; switching the layout moves to the first
  page. The paging buttons target the visible view with `snap` = its row/cell height (same technique as
  `FilePickerPage.qml`). T018's tile checks pass.
- [X] T023 [US2] Restyle `qml/PageButtons.qml`: keep its properties and `pageBy()` logic untouched; replace the two
  `InkButton`s by two `IconButton`s (`text: qsTr("Page up")`/`qsTr("Page down")`, icons `chevron-up`/`chevron-down`,
  `enabled: bar.canUp`/`canDown`, `onClicked` as before). Run the whole QML suite (`qml_file_picker` finds these two
  controls by `text` and `clicked`): everything passes; then run `qml_note_list` completely.
- [X] T024 [US2] Gate wiring for the device (R-9): in `qml/Style.qml` keep `topInset` a single `readonly property real
  topInset: 0` with a comment that it is the height of the system status bar when the system draws it over the app
  (decided in T026); make no other change. (This task exists so that T026 is a one-line edit.)
- [X] T025 [US2] Version: set `VERSION` to `1.7.0` and add a `## 1.7.0` entry to `CHANGELOG.md` (the note list in the
  device's look: top bar with home/menu/search/new, filter and sort row, count line, rows; a Tile layout with a toggle,
  remembered across restarts; the UI typeface is Roboto when the reader provides it). `scripts/check-version-bump.sh`
  passes; run `scripts/pre-merge.sh`.
- [ ] T026 [MANUAL] [GATE] [US2] Package, install, and run quickstart §3 slice 2 (rows 2.1–2.10). Record in
  `checklists/device-results.md`: the `font.ui` line from `pbnotes.log` (`roboto=0|1`) and whether the system status bar
  overlays the app (screenshot 2.1). If it overlays, set `topInset` in `qml/Style.qml` to its height (the built-in
  screens show about 135 px) and repeat 2.1. Record notes per page and tiles per row against the baseline
  (SC-007); adjust `Style.tileHeight`/`rowHeight` only if a row of the walkthrough fails.
- [ ] T027 [US2] [GATE-ONLY: only if T026 recorded `roboto=0`] Bundle Roboto (Apache License 2.0) the way Noto Emoji is
  bundled: add `third_party/roboto/` with Regular, Medium and Bold TTFs and the license text `LICENSE-2.0.txt`;
  `NOTICE` gets a section whose heading starts with the component name `Roboto`; `licenses/manifest.txt` a `component`
  block (`id: roboto`, `kind: component`, `name: Roboto`, `purpose: User-interface typeface`, `license: Apache License 2.0`,
  `holder: Google`, `text: third_party/roboto/LICENSE-2.0.txt`) — `tests/unit/test_license_consistency` must pass;
  `scripts/package-device.sh` installs the TTFs to `applications/` and lists them in its header comment and its input
  check; `app/main.cpp` loads them with `QFontDatabase::addApplicationFont` before choosing the family (host: path from
  a compile definition like `PBNOTES_EMOJI_FONT`). Bump `VERSION` to `1.7.1` with a CHANGELOG entry. Skipped, and
  reported as skipped, when T026 recorded `roboto=1`.

**Checkpoint**: after T026 (and T027 if needed) the note list slice can be merged and tagged `v1.7.0` (or `v1.7.1`).

---

## Phase 4: User Story 3 — Dialogs and pop-up menus look like the device's own (Priority: P2) — slice 3, 1.8.0

**Goal**: every dialog and pop-up has the heavy rounded outline, the title with close icon, the footer or pointer of the
reference, with the same interfaces as before.

**Independent Test**: trigger every dialog and pop-up (confirmation, text entry, choice list, date/time picker, emoji
picker, selection menu, "/" menu, conflict notice) and compare each with `scr0005`, `scr0004` and `scr0007`; every
action still works (quickstart 3.1–3.5).

- [X] T028 [US3] Write `tests/qml/ui_dialogs.qml` first and register it as `qml_ui_dialogs` (page argument
  `${CMAKE_SOURCE_DIR}/qml/ConfirmDialog.qml`; the driver instantiates the other dialogs itself with
  `import "../../qml"`). It MUST fail now. Checks: a `DialogFrame` with `title: "Discard changes?"` has a close icon
  control (`text` "Close"), a rule under the title, a footer with the given cells split by a vertical rule, the
  dismiss cell first, a scrim that swallows a tap outside the box (no `clicked` reaches an item underneath), a 4 px
  outline of radius 18, and `closeRequested` on the close icon; `ConfirmDialog` (title, text, `acceptText` "Discard",
  `cancelText` "Keep editing", `open()`) shows exactly two footer buttons, "KEEP EDITING" left of "DISCARD" (the
  strings stay mixed case, the capitalization is the style), `rejected` on the left one, `accepted` on the right one,
  `showCancel: false` shows one; `TextEntryDialog` shows an `InkField` with the label above, a thin rounded outline,
  `errorText` below, `accepted(text)` on the footer accept and on Return, the box in the upper part of the window
  (`y ≤ 140`), and keeps its `open(initial)`/`close()` behavior; `ChoiceDialog` (a `PopupBubble`) with an `anchor`
  item shows its pointer within 24 px horizontally of the anchor's centre, marks the current row, calls
  `chosen(value)` and closes on a row tap, pages inside with chevrons when more rows than fit (`Page up`/`Page down`
  by text), closes on a tap outside; `SelectionMenu` (items `[{id:"copy",label:"Copy"},{id:"all",label:"Select all"}]`)
  shows icon-only entries whose `text` are "Copy" and "Select all" and emits `chosen("copy")`; `SlashMenu` shows
  icon + label rows with `RowDivider`s between them and keeps its two-column-free single column; `DateTimePicker` and
  `EmojiPicker` have the same frame (title, close, footer); every dialog's `visible` is false until `open()` and the
  device's Back does nothing here (Back is handled by the pages; covered by `editor_flow.qml`).
- [X] T029 [P] [US3] Create the slice-3 icons (list in contract §5, added to `qml/qml.qrc`): `bubble-pointer`
  (a 40 × 24 triangle, 4 px stroke, open at its base; viewBox `0 0 40 24` — the one exception to the 96 grid), `copy`,
  `cut`, `paste`, `select-all`, `trash`, `undo`, `redo`, `check`, and the twelve "/"-menu icons `slash-bullet_list`,
  `slash-table`, `slash-info_box`, `slash-link`, `slash-bold`, `slash-italic`, `slash-underline`, `slash-strikethrough`,
  `slash-details`, `slash-quote`, `slash-code_block`, `slash-emoji` (named after the entry ids of
  `core/src/slash.cpp`). Update contract §5 of `contracts/ui-components.md` with the final list in the same commit.
- [X] T030 [US3] Create `qml/DialogFrame.qml`, `qml/FooterButton.qml` and `qml/InkField.qml` per contract §3 (add each to
  `qml/qmldir` and `qml/qml.qrc`): `DialogFrame` — `anchors.fill: parent`, `z: 1000`, `visible: false`,
  a `MouseArea` that swallows touches, a white rectangle at opacity 0.6, a centred box (width
  `min(parent.width - 80, 900)`, a `Style.heavyWidth` black border, radius 18) with a bold `Style.titleSize` title on the
  left and a `close` `IconButton` (`text: qsTr("Close")`) on the right over a `Style.ruleWidth` rule, the default
  content slot, and a footer of equal-width `FooterButton` cells split by a 1 px vertical rule and a 1 px rule above
  (the first entry of `footer` is the dismiss cell); signals `closeRequested()` and `footerClicked(string role)`.
  `FooterButton` — borderless, `font.capitalization: Font.AllUppercase`, `Style.footerSize`, height 120, light-grey
  pressed fill. `InkField` — a `Text` label above a `TextField` with a 2 px rounded (radius 10) outline, an `errorText`
  line (bold, "✗ " prefix as today) and `accepted()`/`textEdited()`. T028's `DialogFrame` checks pass.
- [X] T031 [US3] Restyle `qml/ConfirmDialog.qml` and `qml/TextEntryDialog.qml` on `DialogFrame`, **keeping every property,
  function and signal listed in contract §3** (including `showCancel`, `open(initial)`, `close()`, the
  `Qt.inputMethod` show/hide, the `field.forceActiveFocus()` and `onTextChanged: root.errorText = ""` behavior and
  the box in the upper part for the text entry). The existing tests that use them (`editor_flow`, `editor_slash`,
  `file_picker`, `settings_sync`) must still pass unchanged. T028's dialog checks pass.
- [X] T032 [P] [US3] Restyle `qml/DateTimePicker.qml` and `qml/EmojiPicker.qml` on `DialogFrame` (view the two files
  first; their signals `accepted(text)`/`cancelled()` and `chosen(emoji)`/`cancelled()` and every property stay): the
  existing inner `InkButton`s become `FooterButton`s in the footer where they are actions (Cancel, OK) and plain
  tappable cells (a bordered cell is not needed) for the emoji grid and the stepper values (the `+`/`−` steppers stay borderless text cells in this slice). `editor_slash.qml`
  must still pass.
- [X] T033 [US3] Create `qml/PopupBubble.qml` (add to `qml/qmldir`, `qml/qml.qrc`): a white rectangle with a
  `Style.heavyWidth` border and radius 18, a `bubble-pointer` `Icon` on its top edge (or bottom when `pointerUp` is
  false) centred at `anchorX` clamped to 28 px inside the corners, with a white patch that hides the border under the
  pointer, and a default content slot with 14 px padding. Restyle `qml/SelectionMenu.qml` and `qml/SlashMenu.qml` onto
  it **without touching their `place()`/`open()`/`close()` logic or properties**: the selection menu shows a `Row` of
  icon-only `IconButton`s (icon by entry id: `cut`, `copy`, `paste`, `remove` → `trash`, `all` → `select-all`, `undo`,
  `redo`; an unknown id shows its label), with `text: modelData.label`; the slash menu becomes one column of rows
  (icon `slash-<id>` size 48 + label, `RowDivider` between rows, rows are 96 px high, the column scrolls as before when
  taller than the room). `editor_slash.qml`, `editor_flow.qml`, `reader_select.qml`, `reader_blocks.qml` and
  `reader_title.qml` must pass; fix their lookups only where a test clicked a text button inside these menus (the
  lookups by `text` and `clicked` still match the new controls; change a test only if it fails and say why in the
  commit).
- [X] T034 [US3] Restyle `qml/ChoiceDialog.qml` as a bubble: keep `title`, `choices`, `current`, `open()`, `close()`,
  `chosen(value)` and add `property Item anchor: null`; when `anchor` is set the bubble is placed below it (above when
  there is no room) with the pointer at the anchor's centre, otherwise centred with no pointer; rows 96 px with the
  label, a `check` icon for the current one (bold label) and `RowDivider`s; a tap outside closes; when the rows are
  more than fit in the room the bubble holds `PageButtons` (chevrons) and pages by whole rows. Pass `anchor` from the
  callers: in `qml/pages/NoteListPage.qml` the view label and the sort label (turn the sort toggle into a two-row
  choice "Recently changed" / "A–Z" with `bridge.setSortOrder` so sort is a pop-up like the view) and the menu icon,
  and in `qml/pages/EditorPage.qml` the category control (`categoryDialog.anchor`). Update `tests/qml/note_list.qml`
  (T018) first: the sort check now opens a bubble and picks "A–Z". T028's choice checks and the old tests pass.
- [X] T035 [US3] Version: `VERSION` `1.8.0` and a `## 1.8.0` entry in `CHANGELOG.md` (dialogs and pop-up menus in the
  device's look; no change in what they offer). `scripts/pre-merge.sh` passes.
- [ ] T036 [MANUAL] [US3] Package, install, run quickstart §3 slice 3 (rows 3.1–3.5) with screenshots; record in
  `checklists/device-results.md`; adjust `Style`/`DialogFrame` numbers if a row fails.

**Checkpoint**: after T036 the dialogs slice can be merged and tagged `v1.8.0`.

---

## Phase 5: User Story 4 — The reader and editor use the device's bars and controls (Priority: P2) — slice 4, 1.9.0

**Goal**: top bars with icon-only universal actions, a bottom panel with icon + label tools, the text-size control in the
device's style, `n / total`, the title as a labelled field.

**Independent Test**: open a note in the reader then the editor; every action of 1.8.0 is still reachable; the bars and
panels match `scr0008`/`scr0009`/`scr0003` (quickstart 4.1–4.6).

- [X] T037 [P] [US4] Write `tests/qml/reader_page.qml` first and register it as `qml_reader_page` (page
  `qml/pages/ReaderPage.qml`; stubs: `bridge.openNote` returns a note with a title, 80 paragraph blocks and a
  category; `bridge.fontSize` "medium", `fontPx` 38; add `fontSize` writing to the stub if missing). It MUST fail now.
  Checks: a `TopBar` with the note title (elided, centred) and controls `Back` (pops the stack) and `Edit` (pushes the
  editor with this `noteId`); no control with a border and a text label; a caption line shows "★ Work"-style text
  ("star icon + category") when the note has them; the bottom bar shows the tool `Text size`, the `Page up`/`Page
  down` controls and an indicator `1 / N` (N = `ceil(contentHeight / step)`), which becomes `2 / N` after *Page down*;
  tapping `Text size` opens a panel with `Decrease`/`Increase` round controls, an `InkSlider` with three stops whose
  index follows `bridge.fontSize` (small 0, medium 1, large 2) and the label "9 pt" for medium ("7 pt", "12 pt"
  for small, large); tapping `Increase` sets `bridge.fontSize` to "large" and disables `Increase`; tapping the track
  at the first stop sets "small"; tapping the tool again or outside the panel closes it; the left/right edge tap
  zones still page; at 1404 × 1872 the `flick` item is at least as high as the baseline from T002 (SC-007).
  `reader_select.qml`, `reader_title.qml`, `reader_blocks.qml` keep passing.
- [X] T038 [P] [US4] Extend `tests/qml/editor_flow.qml` first (it fails until T043): the page has a `TopBar` with
  controls `Back`, `Undo`, `Redo`, `Favorite`, `Save` (icon-only, found by `text` and `clicked`); `Favorite` toggles
  `page.favorite` and its icon between `star` and `star-filled`; `Undo`/`Redo` are disabled until there is something to
  undo/redo and call the edit session; `Save` saves and leaves as the old *Save* did; `Back` with changes opens the
  "Discard changes?" dialog, without changes leaves; the bottom panel holds `Attach` (disabled when
  `attachments.supported` is false), `Category` and — only for an existing note (`noteId > 0`) — `Delete` (opens the
  "Delete this note?" dialog); the panel sits directly above the keyboard (its bottom equals the page height minus
  `keyboardHeight`); the title is a labelled field with `objectName: "titleField"` still present, Return in it moves to
  the text area; **no** `Cancel`, `Save`, `Favorite`, `Attach`, `Delete` or category control with a border remains; the
  existing assertions of the file (save arguments, category choice, discard) still hold.
- [X] T039 [P] [US4] Create the slice-4 icons: `pencil`, `save` (a downward arrow into a tray, as the download icon of the
  reference), `attach` (paperclip), `tag`, `text-size` (an "A" with a small gear), `minus-circle`, `plus-circle`,
  `info`; add to `qml/qml.qrc`; update contract §5. (`undo`, `redo`, `trash`, `star` come from earlier slices.)
- [X] T040 [US4] Create `qml/ToolButton.qml`, `qml/BottomPanel.qml` and `qml/InkSlider.qml` per contract §3 (add to
  `qml/qmldir`, `qml/qml.qrc`): `ToolButton` — icon (56) over a one-line caption-size label (elided), `text`, `icon`,
  `toggled`, `enabled`, `clicked()`, hit area at least 96 high; `BottomPanel` — a row with a 1 px hairline on top and
  its tools evenly spaced (`Row` with equal cells); `InkSlider` — `stops`, `index`, `label`, `moved(index)`; a hairline
  track with 12 px end dots, the ring knob at the current stop, round `minus-circle`/`plus-circle` `IconButton`s
  (`text` "Decrease"/"Increase", disabled at the ends) that move one stop, a tap on the track moves to the nearest
  stop, and the `label` text right of the plus button. Add the checks for these to `tests/qml/ui_style.qml` first
  (hit areas, stop arithmetic, disabled ends, tap on track) and see them fail before writing the components.
- [X] T041 [US4] Extend `qml/PageButtons.qml` with `readonly property int pageCount` and `pageNumber` and a `property
  bool showIndicator: false`: `step = snap > 0 ? floor(target.height / snap) * snap : max(100, target.height - 80)`;
  `pageCount = max(1, ceil((contentHeight) / step))` (for a target with `unsplittable` blocks the same estimate is
  used — it is only a label); `pageNumber = min(pageCount, floor((contentY - originY) / step + 0.5) + 1)`; when
  `showIndicator` is true a caption "n / total" sits between the two chevrons. `pageBy()` and `canUp`/`canDown` do not
  change. Covered by T037's indicator checks.
- [X] T042 [US4] Restyle `qml/pages/ReaderPage.qml`: replace the `top` row and `rule` by a `TopBar` (`Back` chevron,
  note title from `page.note.titleText || page.note.title`, `Edit` `pencil` pushing the editor); under it a one-line
  caption row (`star-filled` `Icon` when favorite, category) of caption size, shown only when either exists; replace
  the footer by a 92 px bottom row: the `Text size` `ToolButton` (icon `text-size`) at the left, `PageButtons` with
  `showIndicator: true` at the right; the `Text size` tool toggles a panel directly above the bottom row (hairline top,
  white, no animation) holding an `InkSlider` (`stops: 3`, `index` from `bridge.fontSize`, `label:
  Style.ptFor(bridge.fontPx) + " pt"`, `onMoved` → `bridge.fontSize = ["small","medium","large"][index]`), which closes on
  a second tap of the tool or a tap on the page outside it. The `Flickable id: flick` (keep `objectName: "flick"`),
  the edge tap zones, the selection menu, handles, `unsplittable` logic, `StackView.onActivated` reload and every
  other part stay as they are. Keep the `message` notice position (above the bottom row). T037 passes and
  `reader_select`, `reader_title`, `reader_blocks` still pass.
- [X] T043 [US4] Restyle `qml/pages/EditorPage.qml`: replace the `top` column by a `TopBar` (left `Back`; title
  `page.noteId > 0 ? qsTr("Edit note") : qsTr("New note")`; right `Undo`, `Redo` (both `enabled` from
  `editSession.canUndo/canRedo`, calling the existing `undoStep()`/`redoStep()`), `Favorite` (`star`/`toggled:
  page.favorite`), `Save` (`save` icon, `page.save()`), `Back` calls `page.requestLeave()`), then a labelled title
  field: the label "Title" above the **existing** `TextField id: titleField` kept with all its handlers, `MouseArea`,
  `SelectionHandles`, `objectName`, `persistentSelection` and history wiring, only its `background` becomes the
  2 px radius-10 outline of `InkField` (use `Style`); the `message` and "attachments need a newer Notes app" texts stay
  below it; a `BottomPanel` anchored to the page bottom minus `page.keyboardHeight` with `ToolButton`s `Attach`
  (`attach`, `enabled: attachments.supported`, `page.openPicker()`), `Category` (`tag`, label is the current category or
  "No category", opens the category choice with `categoryDialog.anchor` = the tool) and `Delete` (`trash`, visible only
  when `page.noteId > 0`, hides the keyboard and opens `deleteDialog`); the `Flickable` text area now ends above the
  panel (bottom margin = keyboard height + panel height). Do not change `menuItems()`, `placeMenu()`, the "/" menu code
  or `applyHistory` and friends. T038 passes; `editor_slash.qml` and `editor_flow.qml` fully pass.
- [X] T044 [US4] Version: `VERSION` `1.9.0` and a `## 1.9.0` entry in `CHANGELOG.md` (reader and editor in the device's look:
  icon controls and a bottom panel, the text size as a slider with its size in points, page position as n / total; no
  change to notes). `scripts/pre-merge.sh` passes.
- [ ] T045 [MANUAL] [US4] Package, install, run quickstart §3 slice 4 (rows 4.1–4.6) with screenshots; record in
  `checklists/device-results.md` including the reader's visible lines per page against 1.5.0 (SC-007) and the `pt`
  labels against the device's own size control; adjust the `ptFor` formula or bar heights only if a row fails.

**Checkpoint**: after T045 the reader/editor slice can be merged and tagged `v1.9.0`.

---

## Phase 6: User Story 5 — Settings, setup, information screens and the attach browser follow the same style (Priority: P3) — slice 5, 1.10.0

**Goal**: the remaining screens use the same top bar, rows, fields and switch; the bordered `InkButton` is removed from the
app.

**Independent Test**: walk every screen: no bordered text button anywhere, every earlier action works, every screen fits
all four orientations (quickstart 5.1–5.5).

- [X] T046 [P] [US5] Update `tests/qml/settings_sync.qml` first (it fails until T049): the Settings page has a `TopBar`
  titled "Settings" with `Back`; the Auto Sync row has an `InkSwitch` (found by `objectName: "autoSyncSwitch"`) showing
  "on" by default, a tap on it (or on its row) flips `bridge.syncAfterSave` and the pill reads "off", and again back; the
  explanatory text is still there; rows are separated by `RowDivider`s; *Forget account* is a row action that opens the
  confirmation (`enabled` false when `bridge.configured` is false); rotation is a row whose value text cycles
  auto → portrait → landscape on a tap; no control with a border and a text label.
- [X] T047 [P] [US5] Write `tests/qml/setup_page.qml` and `tests/qml/about_pages.qml` first and register them as
  `qml_setup_page` (page `qml/pages/SetupPage.qml`) and `qml_about_pages` (page `qml/pages/AboutPage.qml`; the driver
  also loads `LicenseTextPage.qml` and `ThirdPartyPage.qml`); stubs gain what these pages read (`bridge.saveAccount`,
  `bridge.testConnection` or what `SetupPage.qml` calls — read it; `about.*` members: `appName`, `version`, `author`,
  `copyright`, `licenseName`, `appLicense()`, and the third-party list the page reads). Checks: each page has a `TopBar`
  with an upper-case title and (except the set-up page, the app's first screen when unconfigured) a `Back` that pops;
  the setup page has three `InkField`s with labels "Server address", "User name", "App password", the password field
  hides its text until the `Show password` icon control is toggled, a footer of two `FooterButton`s `Test connection`
  and `Save` with `Save` disabled until a test succeeded, the explanatory texts stay; the About page lists name,
  version, author, copyright, license as rows and has rows `License` and `Third-party licenses` that push their pages;
  the license pages page with the existing `PageButtons`; no control with a border and a text label on any of them.
- [X] T048 [US5] Update `tests/qml/file_picker.qml` first (fails until T052): `Cancel` is the `Back` icon of a `TopBar` titled
  "Attach" (still found by the text "Cancel" — give the back `IconButton` `text: qsTr("Cancel")` on this page), `Up`
  is a `chevron-up`-style `IconButton` (`text: "Up"`) in the bar, disabled at the top level, and the layout toggle is
  an `IconButton` with `objectName: "layoutToggle"` in the bar; all assertions about listing, Up at the top, tile
  and list taps, whole-row paging, the 400 ms tap guard, the oversize message, the confirmation and the stored layout
  stay exactly as they are.
- [X] T049 [P] [US5] Write the source scan first: `scripts/check-ui-style.sh` (bash, `set -u`, same shape and
  `--self-test` mode as `scripts/check-constitution.sh`) that fails when any file under `qml/` contains `InkButton`
  (as a type use or an `InkButton.qml` listing) or a property `bordered:`, or a `Button {` (Qt Quick Controls button)
  that is not inside `qml/IconButton.qml`, `qml/ToolButton.qml` or `qml/FooterButton.qml`; its self-test feeds it a
  temporary tree containing each bad pattern and one good tree. Register `check_ui_style` and `check_ui_style_selftest`
  in `tests/CMakeLists.txt` next to the `check_constitution` entries. It fails now (many `InkButton`s remain).
- [X] T050 [P] [US5] Create the slice-5 icons: `eye` (and `eye-filled`), `folder`, `info` (if not yet), `chevron-right`,
  `download`; then create `qml/InkSwitch.qml` per contract §3 (add to `qml/qmldir`, `qml/qml.qrc`): a rounded pill 160 × 76 with a 3 px
  outline, the chosen half filled black with white "on"/"off" text, the other half white, a tap anywhere on the pill
  toggles, `checked`, `toggled(checked)`, `text`, `objectName` set by the page. T046's switch checks pass once the
  page is restyled (T051).
- [X] T051 [US5] Restyle `qml/pages/SettingsPage.qml`: `TopBar` (`Back`, title "Settings"); below it a `Flickable` of
  rows separated by `RowDivider`s with section captions in capitals ("Certificates", "Screen rotation",
  "Synchronization", "Account", "About"); text blocks stay as they are in content; rotation is a tappable row (label
  left, current value right with `chevron-right`); Auto Sync is a row with the label, the explanatory text under it and
  an `InkSwitch` (`objectName: "autoSyncSwitch"`) bound to `bridge.syncAfterSave`; *Forget account* and *About this
  app* are tappable rows; the `✗` message stays. Paging of the flickable stays as it is. T046 passes.
- [X] T052 [US5] Restyle `qml/pages/SetupPage.qml` (read it first; keep every function: `runTest`, `invalidateTest`,
  `reveal`, `keepEntries`, the Wi-Fi wait, the draft keeping): `TopBar` titled "Set up" without a back control; the
  three entries as `InkField`s keeping their `objectName`s, `echoMode`, `inputMethodHints` and handlers; the password's
  Show/Hide as an `eye` `IconButton` (`text` "Show"/"Hide", `toggled: page.showPassword`); `Test connection` and
  `Save` as a two-cell footer of `FooterButton`s anchored above the keyboard; result text and notes as today. T047's
  setup checks pass.
- [X] T053 [P] [US5] Restyle `qml/pages/AboutPage.qml`, `qml/pages/LicenseTextPage.qml`, `qml/pages/ThirdPartyPage.qml` and
  `qml/pages/ImagePage.qml`: each gets a `TopBar` with `Back` and an upper-case title (About: "About"; license pages: the
  content title elided; picture page: no title, a translucent-free white bar with `Back` only); About's rows are
  hairline-separated name/version/author/copyright/license rows and two tappable rows (License, Third-party licenses);
  the license and third-party pages keep their paragraph lists, `topIndex` restore and `PageButtons`; the picture page
  keeps the `Image` filling the area below the bar. T047's about checks pass.
- [X] T054 [US5] Restyle `qml/pages/FilePickerPage.qml` (feature 009 behavior untouched): `TopBar` titled "Attach" with `Back`
  (`text: qsTr("Cancel")`, pops), `Up` (`chevron-up`) as a right-side control, and the layout toggle `IconButton`
  (`objectName: "layoutToggle"`, `grid`/`list` icons as the drawn glyph is today) also on the right; the path line, notice,
  rule, list, grid, tile delegates, preview images, tap guard, confirmation `ConfirmDialog` and `PageButtons` snap logic
  stay as they are (the `Grid`/`Column` glyph code of the old toggle is removed). T048 passes.
- [X] T055 [US5] Remove the old button: replace every remaining `InkButton` in `qml/` (`grep -rn InkButton qml/` must list
  none): in `qml/EmojiPicker.qml` and `qml/DateTimePicker.qml` (cells), `qml/TableRowView.qml`/`AttachmentView.qml`
  (their *Download*/*Open* actions become `FooterButton`-style borderless text actions or an `IconButton` with a
  `download`/`attach` icon — add the icon if needed) and any other hit; delete `qml/InkButton.qml`, its line in
  `qml/qmldir` and `qml/qml.qrc`. T049's scan passes; the whole suite passes (`qml_*`, `app_self_test`).
- [X] T056 [US5] Version: `VERSION` `1.10.0` and a `## 1.10.0` entry in `CHANGELOG.md` (settings, set-up, about, licenses and
  the attach browser in the device's look; Auto Sync as a switch; no bordered text buttons remain). `scripts/pre-merge.sh`
  passes.
- [ ] T057 [MANUAL] [US5] Package, install, run quickstart §3 slice 5 (rows 5.1–5.5) with screenshots, **including the walk of every
  screen in all four orientations (SC-006)** and the check that every action of 1.5.0 still works (SC-004); record in
  `checklists/device-results.md`.

**Checkpoint**: after T057 the whole feature is complete and can be merged and tagged `v1.10.0`.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T058 [P] Update `README.md` (feature list and any screen description) and `INSTALL.md` only where they describe the old
  look or the old icon; the About "third-party" list needs no change unless T027 ran.
- [X] T059 [P] Re-check `contracts/ui-components.md` against the finished code (component names, properties, the final icon
  list) and correct the contract where the code legitimately differs; the contract is the reference for the next feature.
- [X] T060 Run the full local gate once more on the final state: `scripts/pre-merge.sh` (build, all tests including
  `qml_*`, `check_ui_style`, `check_constitution`, `app_self_test`, secret scan, version bump).
- [X] T061 Tick off the success criteria in `checklists/device-results.md`: SC-001 (blind icon check), SC-002 (structural
  elements per screen against the references), SC-003 (`check_ui_style` and the walk), SC-004 (action list), SC-005 (taps to
  create–search–open), SC-006 (orientations), SC-007 (notes per page and reader height against `checklists/baseline.md`),
  SC-008 (no animation: the code contains no `Behavior`, `Transition` with duration, `NumberAnimation` or `PropertyAnimation`
  in `qml/`: `grep` and record), SC-009 (a note, setting and attachment present after update, sync unchanged), SC-010 (each
  slice's device row recorded). List every open [MANUAL] item.

---

## Dependencies & Execution Order

- T001 first; T002 may run beside it (both only read). Everything else depends on T001 being green.
- **US1** (Phase 2): T003 → T004 → T005 → T006. Independent of every other story; it can be merged alone.
- **US2** (Phase 3): the foundation chain T007 → T008 → (T009 ‖ T010) → T011; T012 is independent of QML (needs only T001).
  The setting chain T013 → T014 and T015 → T016 is independent of the QML chain (T016 needs T014). T017 needs T016's
  property name only by convention (the stub plays the bridge) and can run beside T009–T011. T018 needs T017; T019–T022
  need T011 (components), T016 (property, at runtime only), T018 (tests) and are done in order because they edit the same
  file: T019 → T020 → T021 → T022; T023 (PageButtons) may run after T011 and must precede the final T018 run. T024 any time
  after T008. T025 after T022/T023. T026 [MANUAL] after T025; T027 only after T026's result.
- **US3** (Phase 4) needs US2's foundation (T008–T011) and `NoteListPage.qml`/`EditorPage.qml` for T034's call sites.
  T028 first; T029 ‖ T028; T030 needs T029's `close`; T031 → (T032 ‖ T033) → T034 (T034 edits `NoteListPage.qml` and
  `EditorPage.qml`, so not in parallel with US4's page work). T035 after T034; T036 [MANUAL].
- **US4** (Phase 5) needs US3's `PopupBubble`/dialogs for the editor's category choice and delete dialog (T034). T037 ‖ T038
  ‖ T039; T040 needs T039 and its extension of `ui_style.qml` first; T041 after T040; T042 needs T040, T041, T037; T043
  needs T040, T038 (and edits `EditorPage.qml` only after T034). T044 after T043; T045 [MANUAL].
- **US5** (Phase 6) needs US2–US4's components. T046 ‖ T047 ‖ T048 ‖ T049 (tests in different files), T050 ‖ T049; T051 needs
  T046 and T050; T052 needs T047; T053 needs T047; T054 needs T048; T055 needs T049 and **all** of T051–T054 and every
  earlier restyle of a page that still used `InkButton`. T056 after T055; T057 [MANUAL].
- Polish needs the stories it documents. T060 and T061 last.

### Parallel opportunities

- Phase 3: T009 ‖ T012 ‖ T013 ‖ T015 ‖ T017 (different files), then T010/T011 ‖ T014 ‖ T016.
- Phase 4: T028 ‖ T029; T032 ‖ T033.
- Phase 5: T037 ‖ T038 ‖ T039.
- Phase 6: T046 ‖ T047 ‖ T048 ‖ T049 ‖ T050; then T052 ‖ T053 ‖ T054 (different files) after their tests.
- Polish: T058 ‖ T059.

## Implementation Strategy

- **MVP = Phase 1 + Phase 2 (US1)**: T001–T006. It answers the one concrete complaint in the request (the framed icon), is
  a few hours of work, and ships as `v1.6.0` without touching any QML.
- Then **US2** (the note list and the foundation): the largest step and the one that tunes every shared number against the
  device; the two device gates (typeface, status bar) are settled here so slices 3–5 inherit them. Merge and tag it before
  starting US3.
- Then US3 (dialogs, menus), US4 (reader, editor), US5 (the rest and the removal of `InkButton`). Each slice ends with the
  whole suite green, its version bump and its device walkthrough; a slice is not started until the previous one's device row
  is recorded (SC-010).
- Commit per task or per coherent group, test first. When a device row fails, the fix is a change of a `Style` number or a
  component, never a new feature; record both the failure and the fix in `checklists/device-results.md`.
- Open risks and where they surface: the `qmldir` + manual ARM `rcc` (T008, fails loudly in `app_self_test`); the SVG
  plugin on the reader (T026 row 2.1; the label fallback keeps the app usable); reader text area height (T037/T045);
  pointer placement in landscape (T036 rows 3.3–3.4).
