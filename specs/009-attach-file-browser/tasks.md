---

description: "Task list for Attach File Browser"
---

# Tasks: Attach File Browser

**Input**: Design documents from `/specs/009-attach-file-browser/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/qml-bridge.md, quickstart.md

**Tests**: REQUIRED. The constitution makes Test-First non-negotiable (Principle I): every test task
below MUST be written and observed FAILING before the implementation task(s) it names (a test that
does not compile against the missing header or member counts as failing). The one exception is
Phase 2, which pins down the behavior the file browser has **today** (it has no page test yet) and
must PASS on the untouched page; those tests then guard SC-008 while the page changes. Unit tests
under `tests/unit/` are picked up by the glob in `tests/CMakeLists.txt`; bridge tests are listed by
name in the bridge loop of `tests/CMakeLists.txt`; a QML page test needs an
`add_test(... test_qml_pages <driver.qml> <page.qml>)` entry in the QML block of the same file.

**Organization**: Tasks are grouped by user story. Paths are relative to the repository root. This
feature builds on features 001–008; all existing tests must stay green. The core library is not
touched (no `core/` change except the one setting in US2).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US3 as in spec.md
- **[MANUAL]**: needs a person or the reader; record the result in the named file. An implementing
  agent skips these and reports them as open

## Conventions that apply to every task

- C++17. `core/` stays Qt-free (`scripts/check-constitution.sh` R3); Qt types appear only in
  `bridge/`, `app/` and `qml/`.
- **No names in logs** (FR-025): no log line, `qDebug`/`qWarning`, may carry a file or folder name or a
  path. Counts and codes only. Log keys must not be, start or end with `url`, `path`, `file`, `name`
  or `address` (R6): use `count`, `reason`.
- **Numbers from the design** (apply in code, not by convention): the seven folder names, spelled
  exactly `Audio Books`, `Downloads`, `Music`, `My pictures`, `Notes`, `Photo`, `screens`; setting
  `picker_view` with values `list` / `tile`, default `list`; tile grid `columns = max(2, floor(width /
  270))`, `cellWidth = width / columns`, `cellHeight = 250`; preview area `170` px high; tile name at
  most two lines, 24 px; list rows stay `104` px; tap guard `400` ms after a folder change; preview
  cache `8 * 1024 * 1024` bytes; decoder pool `maxThreadCount = 1`; pixel guard stays the existing
  `40000000`.
- Browsing is read-only (FR-024): nothing in this feature opens a file for writing, creates, renames
  or deletes anything outside the app's own data folder. The preview never replaces the original when
  attaching (FR-022).
- UI strings go through `qsTr`. Interfaces are those of `contracts/qml-bridge.md`; if a task finds a
  contract wrong, fix the contract in the same commit.
- A new header with `Q_OBJECT` (`bridge/include/file_preview_provider.h`) must be added in **both**
  places in `bridge/CMakeLists.txt`: the `pbnotes_moc(...)` list (ARM) and the host `add_library`
  header list. No new QML file, so `qml/qml.qrc` does not change.
- Test fixtures are generated in the test (temporary directories, tiny generated images); no real user
  files, no binary fixtures committed.

---

## Phase 1: Setup

- [X] T001 Build the host target and run the whole suite on the untouched branch
  (`cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-host && ctest
  --test-dir build-host --output-on-failure`). Every test must pass (including `app_self_test` and the
  `qml_*` tests); if one does not, stop and report it — later failures must be attributable to this
  feature.

---

## Phase 2: Foundational (a page test for today's file browser — used by all three stories)

**Purpose**: the file browser page has no test today. Pin its current behavior so the changes of the
three stories cannot silently break attaching (SC-008). No production code changes in this phase.

- [X] T002 [P] Extend the `attachments` stub in `tests/qml/stubs.qml` with what `FilePickerPage.qml`
  reads: `rootDirectory` (string, `"/root"`), `maxBytes` (number, `1000000`), `pickerView` (string,
  writable, default `"list"`), and `listDirectory(path)` returning a fixture by path: `/root` → three
  folders `Music`, `Photo`, `screens` (no files); `/root/Photo` → a subfolder `Trips`, then 40 files
  named `img01.jpg` .. `img40.jpg` (each with `size: 120000`, `image: true`, `previewId: 1 .. 40`),
  `notes.pdf` (`size: 50000`, `image: false`, `previewId: 0`) and `huge.bin` (`size: 2000000`); `/root/Music`
  → empty; `/root/Photo/Trips` → two files. Entries are maps `{name, dir, size, image, previewId}` with
  folders first. Add a property `rootIsEmpty` (bool, default false) that makes `/root` return `[]`. Add
  `function formatSize(bytes)` to the `bridge` stub (returns `bytes + " B"`) if it is not there.
- [X] T003 Write `tests/qml/file_picker.qml` (driver, same pattern as `tests/qml/settings_sync.qml`,
  window `1404 x 1872`, the page hosted in a `StackView` so `StackView.view.pop()` works, a `Connections`
  to the page's `picked` signal) with the **list view** cases that hold today, against the fixture of T002:
  the page starts at `attachments.rootDirectory`; rows show kind label, name and size and are ordered
  folders first; tapping a folder shows its entries and the path line; `Up` returns to the parent;
  tapping `notes.pdf` opens the dialog titled "Attach this file?" with name and size, accepting it emits
  `picked("/root/Photo/notes.pdf")` and pops the page, cancelling emits nothing; tapping `huge.bin` shows
  the "larger than the ... limit" message and no dialog; `Cancel` pops; Page down moves `contentY` by a
  multiple of `104` and no row is cut off (`contentY % 104 === 0`); the empty folder `Music` shows "This
  folder is empty or cannot be read.". Use `eq`/`check` helpers and injected mouse presses.
- [X] T004 Register the test in `tests/CMakeLists.txt` (QML block, next to `qml_settings_sync`):
  `add_test(NAME qml_file_picker COMMAND test_qml_pages ${CMAKE_CURRENT_SOURCE_DIR}/qml/file_picker.qml
  ${CMAKE_SOURCE_DIR}/qml/pages/FilePickerPage.qml)` with `ENVIRONMENT "QT_QPA_PLATFORM=offscreen"` and
  `LABELS "bridge"`. Run `ctest -R qml_file_picker`: **it must pass on the unchanged page.** If a case fails
  because the test is wrong, fix the test; if the page behaves differently from the list above, report it.

**Checkpoint**: the existing browser is covered; all earlier tests are green.

---

## Phase 3: User Story 1 - Start in a clean list of useful folders (Priority: P1) 🎯 MVP

**Goal**: the browser's top level lists only the seven named folders; the reader cannot go above it.

**Independent Test**: on a directory with the seven folders, `Autorun.inf`, `install.md` and another
folder, `listDirectory(root)` returns exactly the seven; the page's `Up` is disabled at the root.

- [X] T005 [P] [US1] Write the failing tests in `tests/bridge/test_attachments_bridge.cpp` (next to the
  existing listing tests; the `Env` helper builds the root, set `ac.topFolders` explicitly in the cases
  that need it): with `topFolders = {Audio Books, Downloads, Music, My pictures, Notes, Photo, screens}`:
  (a) a root holding all seven folders, the files `Autorun.inf` and `install.md` and the folders `System`
  and `Other` lists exactly the seven, in case-insensitive name order, no files (FR-001, FR-002, FR-013);
  (b) a folder named `photo` and one named `SCREENS` are listed (FR-003) while `Photos` and `Photo2` are
  not (whole name); (c) a **file** named `Music` is not listed; (d) a link to a directory named `Notes` is
  listed as a directory; (e) only the existing ones of the seven are listed when some are missing; an
  empty list when none exist; (f) inside `Photo` every subfolder and every file is listed, unfiltered, in
  the old order (FR-004); (g) with `topFolders` empty the root is listed unfiltered (existing behavior,
  keeps the older tests valid); (h) `AttachmentsBridge::defaultTopFolders()` returns exactly the seven
  names above, in that order, with "My pictures" spelled that way; (i) a root given with a trailing
  slash (`/x/`) is recognized as the root. The existing listing tests must stay green unchanged.
- [X] T006 [US1] In `bridge/include/attachments_bridge.h` / `bridge/src/attachments_bridge.cpp` add
  `QStringList topFolders` to `Config` (documented: names of the folders listed at the root, compared
  whole-name and case-insensitively, empty = unfiltered), `static QStringList defaultTopFolders()`
  returning the seven names, and the root rule in `listDirectory`: when
  `QDir::cleanPath(path) == QDir::cleanPath(config_.rootDirectory)` and `topFolders` is not empty, keep
  only entries with `e.isDir()` whose `fileName()` equals (`Qt::CaseInsensitive`) a `topFolders` item.
  Update the method's header comment. No logging. T005 passes.
- [X] T007 [US1] In `app/main.cpp` set `attachment_config.topFolders = AttachmentsBridge::defaultTopFolders();`
  next to `rootDirectory` (both platforms). Build the app target; `app_self_test` still passes.
- [X] T008 [P] [US1] Extend `tests/qml/file_picker.qml` with the failing cases: `Up` is disabled
  (`enabled === false`) at `/root` and enabled in `/root/Photo`; tapping `Up` in `/root/Photo` returns to
  `/root` and `Up` is disabled again; from `/root/Photo/Trips` two taps on `Up` lead to `/root` (never
  above it); the root path line shows `/root`; with `rootIsEmpty` the page shows the text "Nothing to
  browse here" and `Cancel` still pops; opening the page again (a fresh instance) starts at `/root` even
  after browsing (US1-6). The existing assertion that `Up` is enabled at a non-root path stays.
- [X] T009 [US1] In `qml/pages/FilePickerPage.qml` add `readonly property bool atTop` (the current path
  equals `attachments.rootDirectory` after removing one trailing `/`), make `Up` `enabled: !page.atTop`
  (replacing the `path !== "/"` test), and show, when `atTop` and `rows.length === 0`, the text
  `qsTr("Nothing to browse here. None of the folders for attachments were found.")` instead of the
  "empty or cannot be read" text (which stays for other folders). Update the file's header comment. T008
  passes; T003's cases still pass.

**Checkpoint**: US1 works on its own. The browser opens on the seven folders and stays inside them.

---

## Phase 4: User Story 2 - Switch between List and Tile view (Priority: P2)

**Goal**: a toggle at the top switches the same folder between today's list and a grid of tiles; the
choice is remembered, also after a restart.

**Independent Test**: in a folder with 40+ entries the toggle shows tiles (about twice as many per page
as rows), toggling back restores the list in the same folder; a fresh page opens in the stored view.

- [X] T010 [P] [US2] Write the failing tests in `tests/unit/test_store_open.cpp` (next to
  `sync_after_save_is_on_by_default_and_accepts_only_on_and_off`): a fresh store returns `list` for
  `get_setting("picker_view")`; `set_setting("picker_view", "tile")` and `"list"` return true and are read
  back, also after closing and reopening the database; `"grid"`, `""`, `"TILE"` and `"1"` return false and
  leave the value unchanged; with the value `tile`, `forget_account()` leaves it `tile`; a database
  created by the previous version (the newest migration test, `test_store_migration_v3.cpp` or its
  successor) reads `list`.
- [X] T011 [US2] Add `picker_view` to `setting_defaults()` (`"list"`) and `valid_setting()` (`list` or
  `tile`, nothing else) in `core/src/note_store.cpp`, and to the documented keys of `get_setting` in
  `core/include/pbnotes/note_store.h`. No schema change, no version bump. T010 passes.
- [X] T012 [P] [US2] Write the failing tests in `tests/bridge/test_attachments_bridge.cpp`: `pickerView`
  reads `list` on a fresh install (no account); setting it to `tile` stores it, emits `pickerViewChanged`
  once and is read back by a **new** `NotesBridge` + `AttachmentsBridge` on the same data folder; setting
  the same value again emits nothing; setting `grid` is ignored (value and signal unchanged).
- [X] T013 [US2] In `bridge/include/attachments_bridge.h` / `bridge/src/attachments_bridge.cpp` add
  `Q_PROPERTY(QString pickerView READ pickerView WRITE setPickerView NOTIFY pickerViewChanged)`, reading
  and writing the setting `picker_view` through `notes_->store()` (a null store reads `list` and refuses
  writes), emitting `pickerViewChanged()` only when the stored value changed. T012 passes.
- [X] T014 [US2] Extend `tests/qml/file_picker.qml` with the failing cases (window `1404 x 1872`; give
  the stub a `pickerView` that records writes): the toggle button (found by `objectName: "layoutToggle"`)
  exists next to `Cancel` and `Up`; while the list is shown its glyph is the grid symbol
  (`property string glyph` of the toggle reads `"grid"`), while tiles are shown `"list"`; a tap switches the
  view, keeps `path`, the path line and a visible message, shows the first page, and writes
  `attachments.pickerView` (`"tile"` / `"list"`); a fresh page with `attachments.pickerView = "tile"` starts
  as tiles, and with the default as list (US2-4); in `/root/Photo` the tile view has
  `columns === max(2, floor(1404 / 270)) === 5`, `cellHeight === 250`, and the number of tiles shown on the
  first page is **at least twice** the number of rows shown in the list view on the first page (SC-003);
  with the window narrowed to the landscape size `1872 x 1404` columns become 6; tapping a folder tile
  enters it; tapping `img03.jpg` opens the "Attach this file?" dialog with its name and size and accepting
  emits `picked("/root/Photo/img03.jpg")`; tapping `huge.bin` shows the size message; Page down in the
  tile view moves `contentY` by a multiple of `250`; a name of 60 characters is elided (`elide ===
  Text.ElideRight`, `maximumLineCount === 2`) and the dialog shows the full name (US2-7); two taps in
  quick succession on a folder tile (second one on a tile of the new folder) enter only the first folder
  (the second is ignored within `400` ms) while a tap after `450` ms is accepted; `Up`, `Cancel` and the
  toggle are never delayed by the guard.
- [X] T015 [US2] In `qml/pages/FilePickerPage.qml` implement the tile view: `property string view:
  attachments.pickerView === "tile" ? "tile" : "list"`; the toggle `Button` (same border, radius, height
  and colors as `InkButton`, `objectName: "layoutToggle"`, `property string glyph`) at the right end of
  the top row, drawing with `Rectangle`s a 2 x 2 grid of squares when `view === "list"` and three bars
  when `view === "tile"`, with `onClicked` flipping `view`, writing `attachments.pickerView`, resetting
  the position to the first page; the existing `ListView` gets `visible`/`enabled` only when `view ===
  "list"`; a `GridView` (`interactive: false`, `boundsBehavior: Flickable.StopAtBounds`, `cacheBuffer: 0`,
  `reuseItems: false`, `model: page.rows`, `readonly property int columns: Math.max(2, Math.floor(width /
  270))`, `cellWidth: width / columns`, `cellHeight: 250`, clipped, same anchors as the list) shown only
  when `view === "tile"`; the tile delegate (tap area calling the **same** `page.open(entry)`): a 170 px
  area with a drawn folder glyph (tab plus body) for folders or a bordered page glyph with the file's
  extension in capitals (at most four characters) for files, below it the name (24 px, `elide:
  Text.ElideRight`, `maximumLineCount: 2`, `wrapMode: Text.Wrap`), a light gray `#d8d8d8` background
  while pressed; `PageButtons` follows the visible view (`target` and `snap: 104` for the list, `snap:
  250` for the grid, switched by `view`); no animation anywhere. T014's cases (except the tap guard)
  pass.
- [X] T016 [US2] In `qml/pages/FilePickerPage.qml` add the tap guard: record `lastNavigation` (`Date.now()`)
  whenever the folder changes (entering a folder, `Up`), and make `open(entry)` return at once when
  `Date.now() - lastNavigation < 400`; nothing else is delayed. T014's guard case passes; T003's cases
  still pass (they wait longer than 400 ms between navigations, add `450` ms waits in the driver where
  needed).

**Checkpoint**: US1 and US2 work; the browser is usable without previews.

---

## Phase 5: User Story 3 - See what a picture is before attaching it (Priority: P3)

**Goal**: in Tile view image files show a small grayscale preview, prepared off the UI thread for the
visible page only, bounded in memory; everything else keeps its glyph.

**Independent Test**: a folder with images, a PDF and a subfolder in Tile view: the images show previews,
the rest glyphs; the page responds while previews load; List view prepares none.

- [X] T017 [P] [US3] Write the failing tests in `tests/bridge/test_attachments_bridge.cpp`: in a listing
  of `a.JPG`, `b.png`, `c.txt`, `d.webp`, a folder `sub` and a file `e.jpg` with mode `000`: `image` is
  true for `a.JPG` (extension rule is case-insensitive), `b.png` and `d.webp`, false for `c.txt` and the
  folder; `previewId` is non-zero for the readable image entries, pairwise different, and 0 for
  everything else including the unreadable `e.jpg` (skip that part if the process can read it, as the
  existing "locked" test does); a FIFO named `f.jpg` (made with `mkfifo`) is not given an id (only
  regular files are); a second `listDirectory` call returns ids **greater than** all earlier ones and the
  earlier ids no longer resolve in `previewIndex()`; an empty folder yields an empty index.
- [X] T018 [P] [US3] Write the failing tests in a new `tests/bridge/test_file_preview_provider.cpp`
  (pattern: `tests/bridge/test_attachment_image_provider.cpp`, request helper that runs an event loop
  until `finished`, images generated with `QImage::save` into a temporary folder), and add the name to the
  bridge test loop in `tests/CMakeLists.txt` (the header does not exist yet, so it fails to compile):
  (a) a registered 400x200 PNG requested with width 100 gives a 100x50 `Format_Grayscale8` image; a 60x30
  image requested with width 500 stays 60x30 (never enlarged); (b) an id that was never registered, an
  id from a replaced index, and a registered path that was deleted give a null image; (c) a corrupt file
  named `x.jpg` gives a null image; an image described as larger than 40000000 pixels (write a PNG header
  with a huge size, or lower the guard through the same parameter `load_attachment_image` already has) is
  refused before decoding; (d) the cache never holds more than `8 * 1024 * 1024` bytes: request 400
  different 300x300 images (cost about 90 KB each) and assert the provider's `cachedBytes()` accessor stays
  at or below the limit and that a repeated request for the most recent image is answered from the cache
  (assert through `cachedBytes()` and a hit counter `cacheHits()`); (e) after the file is replaced by a
  different image of another size, the same id yields the new image, not the cached old one; (f) a
  response cancelled while it is queued behind a long job never decodes (`decodeCount()` unchanged);
  (g) the file's bytes and modification time are identical after requests; (h) requests run on a worker
  (the response is not finished when `requestImageResponse` returns for an uncached image).
- [X] T019 [US3] Create `bridge/include/file_preview_provider.h` with `class PreviewIndex` (thread-safe:
  `int add(const QString& path)` using a counter that only grows and starting at 1, `void replace(const
  QVector<QPair<int,QString>>&)` or equivalent single-swap API, `QString path(int id) const`, empty for
  unknown ids) and, in `AttachmentsBridge`, a `std::shared_ptr<PreviewIndex> previewIndex() const`, the new
  entry fields `image` (bool; `attachment_kind_for(name)` is `Image` and `e.isFile()`) and `previewId`
  (int; non-zero only if `image` and readable, 0 otherwise) in `listDirectory`, and the index replacement
  per call (clear + fill under one lock). Add the header to the moc list (ARM) and header list (host) in
  `bridge/CMakeLists.txt`. Log nothing. T017 passes.
- [X] T020 [US3] Create `bridge/src/file_preview_provider.cpp` and the rest of
  `bridge/include/file_preview_provider.h`: `class FilePreviewProvider : public QQuickAsyncImageProvider`
  taking the `std::shared_ptr<PreviewIndex>`, a private `QThreadPool` with `setMaxThreadCount(1)`, a
  `QCache<QString, QImage>` with `setMaxCost(8 * 1024 * 1024)` (cost = `sizeInBytes()`) behind a mutex and
  the key `path|size|mtime|width`; `requestImageResponse(id, requestedSize)` resolves the id (unknown →
  an immediately finished null response), answers from the cache on the next event-loop turn, otherwise
  runs a job calling `load_attachment_image(path, requestedSize.width())` (reuse it; do not copy it),
  stores the result, and completes asynchronously; the response implements `cancel()` with an atomic flag
  the job checks before decoding; add the accessors `cachedBytes()`, `cacheHits()` and `decodeCount()` used
  by T018 (documented as test hooks). Response/job classes with `Q_OBJECT` live in the same header like in
  `attachment_image_provider.h`. Add `src/file_preview_provider.cpp` to `PBNOTES_BRIDGE_SOURCES`. Never
  log a path or name; a failed decode is counted only (`preview.decode_failed count=<n>`). T018 passes.
- [X] T021 [US3] In `app/main.cpp` register the provider next to the attachment one:
  `engine.addImageProvider(QStringLiteral("filepreview"), new FilePreviewProvider(attachments.previewIndex()));`
  (the shared pointer keeps the index alive for the engine). Build host and ARM-independent parts; run
  `app_self_test`.
- [X] T022 [US3] Extend `tests/qml/file_picker.qml` with the failing cases, and give `tests/qml/test_qml_pages.cpp`
  (or the driver, if it can) a test-only `QQuickImageProvider` named `filepreview` that returns a 40x30
  gray image for ids 1..30 and a null image otherwise: in the tile view of `/root/Photo` each `img01..img30`
  tile contains an `Image` whose `source` is `image://filepreview/<previewId>` with `sourceSize.width` set,
  `asynchronous: true` and `cache: false`; when its status is `Image.Ready` the file glyph is hidden, when
  it is `Image.Error` (`img31..img40` have no image) the glyph is shown; `notes.pdf`, `huge.bin` and the
  folder `Trips` tiles contain **no** `Image` item (FR-018); only the tiles of the visible page have
  delegates: the number of `Image` items equals the number of visible image tiles, and after Page down
  the items of the first page are gone (FR-020); in the list view no `Image` item exists at all (FR-023);
  switching to the list and back creates tiles again without error.
- [X] T023 [US3] In `qml/pages/FilePickerPage.qml` implement the preview in the tile delegate: for entries
  with `entry.image && entry.previewId > 0` add `Image { asynchronous: true; cache: false;
  fillMode: Image.PreserveAspectFit; sourceSize.width: <preview area width>; source:
  "image://filepreview/" + entry.previewId; visible: status === Image.Ready }` over the file glyph, which
  stays visible while the status is not `Ready` (also on `Error`, FR-019). The preview area is 170 px
  high; the image is centered in it. No `Image` is created for other entries or in the list view. T022
  passes.

**Checkpoint**: all three user stories work.

---

## Phase 6: Polish & cross-cutting concerns

- [X] T024 Review: run `scripts/check-constitution.sh`; grep the diff for `qDebug`, `qWarning` and
  `log::` calls and confirm none carries a file or folder name or a path (counts and codes only, FR-025);
  confirm nothing in the diff opens a file for writing or removes/renames anything (FR-024) and that the
  provider can only reach paths registered in `PreviewIndex` (FR-022); confirm no new dependency, no
  schema change, no network use, and that `picker_view` is not touched by `forget_account`.
- [X] T025 [P] Record the refinement in the feature 002 spec: in
  `specs/002-rich-note-content/spec.md` User Story 5, acceptance scenario 1 ("they can browse the
  device's storage"), add one sentence "Refined by feature 009: the browser starts at a fixed list of
  folders and offers a tile view with previews." Do not rewrite the scenario.
- [X] T026 [P] Update `README.md` (feature list: the file browser's seven folders, List/Tile view, image
  previews) and the attachment paragraph of `INSTALL.md` if it describes the file browser. In
  `docs/security-review.md` add one row to the image-handling table for the preview provider: reads only
  ids the browser listed, regular files with an image extension, 40-million-pixel guard, decoded off the
  UI thread, 8 MB cache, nothing written to disk; evidence `test_file_preview_provider`.
- [X] T027 Release: raise `VERSION` from `1.4.0` to `1.5.0` (MINOR: a user-visible feature, nothing
  breaking), add the `## 1.5.0` entry to `CHANGELOG.md` (Attach: the file browser opens on a fixed list of
  folders — Audio Books, Downloads, Music, My pictures, Notes, Photo, screens — and no longer shows the
  device's top-level files; a toggle at the top switches between List and Tile view and is remembered; in
  Tile view images show a grayscale preview; attaching itself, the size limit and the confirmation are
  unchanged; no change to the server, the data or the sync protocol), rebuild, and run
  `scripts/check-version-bump.sh` as `scripts/pre-merge.sh` does. Tag `v1.5.0` after the merge.
- [X] T028 Run `scripts/pre-merge.sh` (all tests, constitution checks, version bump) on the final tree
  and fix what it reports.
- [X] T029 [MANUAL] Walk `quickstart.md` §3 (all 12 rows) on the reader with the release package. Record
  in `specs/009-attach-file-browser/checklists/device-results.md`; measure row 3 against SC-003 (tiles per
  page versus list rows), row 4 against SC-005 (previews within 10 s) and row 10 against SC-006; if
  SC-003 or SC-005 is not met, tune the tile size (270 / 250 / 170) in `qml/pages/FilePickerPage.qml` and
  repeat. (Done 2026-10-13: owner confirmed it works; see `checklists/device-results.md`, no per-row numbers.)
- [X] T030 Update `specs/009-attach-file-browser/checklists/requirements.md` notes: every FR-001..FR-025
  is covered by a task above (map: FR-001..FR-003 → T005–T007; FR-004 → T005(f); FR-005, FR-006 →
  T008, T009; FR-007 → T003, T014; FR-008 → T014, T015; FR-009 → T003, T014; FR-010, FR-011 → T014, T015;
  FR-012 → T010–T015; FR-013 → T003, T005; FR-014 → T003, T014, T015; FR-015 → T014, T015; FR-016 →
  T014, T015, T029; FR-017, FR-018 → T017–T020, T022, T023; FR-019 → T018, T020, T023, T029; FR-020 →
  T018, T020, T022, T023; FR-021 → T018, T020; FR-022 → T018, T020, T024; FR-023 → T022, T023;
  FR-024 → T024; FR-025 → T024); list the open [MANUAL] tasks.

---

## Dependencies & Execution Order

- T001 first. Phase 2: T002 → T003 → T004 (stubs, then the driver that uses them, then the CMake entry);
  T004 must pass before any story changes the page.
- **US1** (Phase 3): T005 ‖ T008 (tests in different files); T005 → T006 → T007; T008 → T009 (needs T004).
  The bridge half (T005–T007) and the page half (T008–T009) are independent of each other.
- **US2** (Phase 4) needs US1's T009 (same page file). T010 → T011 and T012 → T013 are independent chains
  (core / bridge); T013 needs T011 (the store must accept the key). T014 needs T004 and T002's stub; T015
  needs T014 and **T013 only at runtime** (the stub covers the QML test); T016 follows T015 (same file).
- **US3** (Phase 5) needs US2's tile view (T015) for T022/T023. T017 ‖ T018 (tests in different files);
  T019 needs T017 and adds the header T018 compiles against; T020 needs T019 and T018; T021 needs T020;
  T022 needs T015 and the stub fixture of T002; T023 follows T022. The bridge half (T017–T021) is
  independent of US2's QML work and may run in parallel with it, except that T019 and T013 both edit
  `attachments_bridge.h/.cpp` (do T013 first, a few lines).
- Polish needs the stories it documents or runs. T027 (version bump) comes last before the merge; T028
  after it; T029 on the release package.

### Parallel opportunities

- Phase 3: T005 ‖ T008, then T006/T007 ‖ T009.
- Phase 4: T010 ‖ T012 ‖ T014 (tests in three different files), then T011 ‖ T013 ‖ T015.
- Phase 5: T017 ‖ T018, then T019 → T020 → T021 (bridge) ‖ T022 → T023 (QML, after US2).
- Polish: T025 ‖ T026.

## Implementation Strategy

- **MVP = Phase 1 + Phase 2 + US1** (T001–T009): the browser opens on the seven folders and cannot go
  above them. That alone answers the main complaint (clutter, system files) and can ship as a patch of
  the feature; the release bump comes with the full feature.
- Then US2 (layout toggle and tiles with glyphs: useful without previews, and the stored choice), then
  US3 (previews: the riskiest part for speed and memory, so last and isolated in its own provider that
  can be switched off by removing the `Image` from the delegate without touching the rest).
- Each phase ends with the whole suite green. Commit per task or per coherent group, test first.
- Device-only questions (how the grayscale previews look, tiles per page, preview timing) are not
  blockers; they are answered in T029 and may adjust the three tile constants.
