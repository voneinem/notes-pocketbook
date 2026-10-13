# Research: Attach File Browser

Resolves what the plan's Technical Context left open. Facts were read from this repository (bridge,
QML, tests, specs 001/002/008, the SDK notes); nothing was probed on the device. Items that only the
device can answer are listed in R-9 and become walkthrough steps.

## R-1. Where the seven-folder rule lives (FR-001..FR-006)

- **Findings**: `AttachmentsBridge::listDirectory(path)` is the only code that lists files for the picker
  (`bridge/src/attachments_bridge.cpp`). It uses `QDir::entryInfoList(AllDirs | Files | NoDotAndDotDot,
  DirsFirst | IgnoreCase | Name)`, so hidden entries are already skipped, and drops unreadable and
  dangling entries. The start folder is `Config::rootDirectory` (`/mnt/ext1` on the device, the home
  folder on the host), set by `app/main.cpp`. The page starts at `attachments.rootDirectory`.
- **Decision**: add `Config::topFolders` (`QStringList`, set by `app/main.cpp`). When `path` equals the
  root (compared after `QDir::cleanPath` on both sides), `listDirectory` keeps only directories whose
  file name equals an entry of `topFolders` ignoring case (whole name; a file with such a name is
  dropped because only directories are kept; a link to a directory counts as a directory through
  `QFileInfo::isDir`). An empty `topFolders` means no filter, so the existing bridge tests stay valid and
  tests opt in. Every other path is listed as before.
- **Alternatives considered**: a hard-coded constant in the bridge (the existing tests would then need
  fixtures with the seven names in every case, and device layout facts would sit in generic code); filtering in QML
  (testable only through a page, and the bridge would still hand the full root listing to QML);
  a Settings screen to edit the names (not asked; YAGNI, constitution II).

## R-2. Keeping the reader inside the seven folders (FR-005)

- **Findings**: the page computes the parent path from the string (`parentOf`) and today disables "Up" only
  at `/`. Navigation happens only by tapping listed entries, so the path can leave the root only through
  "Up" or through a link entry that points elsewhere (paths are strings, not resolved).
- **Decision**: "Up" is disabled when the page's path equals the root (same normalization as R-1) and
  `parentOf` of a first-level folder is the root itself. Nothing in the bridge resolves links, so a link
  inside a folder opens like a folder and "Up" returns along the string path (spec edge case).
  The bridge does **not** refuse other paths: `listDirectory` is also called by tests with arbitrary
  paths, and the browser's reach is limited by the UI rather than by a security boundary (the process
  can read those files anyway; the boundary that matters is the preview registry, R-4).
- **Alternatives considered**: refusing every path outside the root in the bridge (breaks the spec's
  link edge case and adds a canonicalization step that behaves differently on vfat); resolving links with
  `canonicalFilePath` (would show the target's real path and could let "Up" leave the root).

## R-3. Entry fields (FR-010, FR-017, FR-018, FR-023)

- **Findings**: entries are `{name, dir, size}`. The reader's rules for "is this an image" are in the
  core: `pbnotes::attachment_kind_for` (jpg, jpeg, png, gif, bmp, webp by extension), used for
  attachments in notes (spec 002).
- **Decision**: entries gain `image` (bool: regular file with an image extension by that rule) and
  `previewId` (integer, present and non-zero only for those entries that are also readable). The
  extension rule stays a case-insensitive extension check; decode failures (a webp the device Qt cannot
  read, a damaged file) are handled by the fallback in R-4, not by guessing from content.
- **Alternatives considered**: sniffing file content to decide (reads every file in the folder at
  listing time; slow on a card); a separate `isImage(path)` call per tile (QML-to-C++ round trips per tile
  and a second place for the rule).

## R-4. Delivering previews (FR-017..FR-023, SC-005, SC-006)

- **Findings**: attachments are shown through `image://attachment/<id>?w=<px>` served by an async provider
  that decodes on `QThreadPool::globalInstance()` using `load_attachment_image` (scaled `QImageReader`,
  EXIF orientation, 40 million pixel guard, 8-bit grayscale). A QML `Image` with a `file:` source would
  decode on the QML image thread at full size unless `sourceSize` is set, offers no cache bound beyond
  Qt's pixmap cache, and ties file names into URLs (characters such as `?`, `#` and `%` break them).
  `QQuickImageResponse::cancel()` is called when an `Image` is destroyed or its source changes.
- **Decision**: a new provider `filepreview` with the same shape:
  1. **Registry**: `PreviewIndex` (shared pointer, mutex-protected) maps `previewId → absolute path`.
     `listDirectory` replaces its content on every call and numbers entries from a counter that only
     grows; an unknown id (an old listing) yields a null image, which the tile shows as the file glyph.
     The provider cannot be asked for a path, only for an id the browser handed out.
  2. **Decoding**: `requestImageResponse(id, requestedSize)` looks the id up, then the cache; on a miss it
     starts a job on a dedicated `QThreadPool` with `maxThreadCount = 1`, which calls
     `load_attachment_image(path, requestedSize.width())`. A cancelled response sets a flag the job
     checks before decoding, so tiles that left the page cost a queue entry, not a decode.
  3. **Cache**: `QCache<QString, QImage>` with `maxCost` of 8 MB (cost = image byte size), key
     `path|size|mtime|width`, guarded by a mutex; hits answer on the next event-loop turn. Nothing is written to disk.
  4. **QML** uses `Image { asynchronous: true; cache: false; sourceSize.width: ...; fillMode: PreserveAspectFit }`
     (`cache: false` so Qt's own pixmap cache does not hold a second copy) and shows the file glyph while
     the status is not `Ready`.
- **Alternatives considered**: using the global thread pool (an attachment download decode or UI-bound jobs
  would queue behind photo decoding, and several decoders at once would use all cores and a lot of RAM on a
  small device); a thumbnail cache directory on the card (writes files the reader did not ask for,
  FR-022); unbounded Qt pixmap caching (SC-006 requires a bound); one `Image` per listed entry created up
  front (see R-5, only visible tiles exist).

## R-5. Tile view layout and paging (FR-010, FR-014, FR-016, SC-003)

- **Findings**: the stage is authored in panel pixels (1404 x 1872 portrait, scaled by 1/devicePixelRatio).
  The list uses 104 px rows, no kinetic scrolling (`interactive: false`) and `PageButtons` with
  `snap: 104`, which moves by whole rows. The list page shows about 15 rows in portrait. A `GridView` is a
  `Flickable` with `contentHeight`/`contentY`, so `PageButtons { target: grid; snap: cellHeight }` works
  unchanged.
- **Decision**: `GridView { interactive: false; cacheBuffer: 0; reuseItems: false }` with
  `columns = max(2, floor(width / 270))` and `cellWidth = width / columns`, `cellHeight = 250` (a 170 px
  preview area plus two lines of name at 24 px). That is 5 columns x 6 rows = 30 tiles in portrait against
  about 15 list rows (2x, SC-003) and 6 x 4 = 24 against about 10 rows in landscape. `cacheBuffer: 0` means
  only on-screen tiles have delegates, hence previews are requested only for the current page (FR-020).
  The numbers are starting values, tuned in the device walkthrough (R-9), because they depend on the
  panel's real size and font rendering.
- **Alternatives considered**: three columns (not twice the list); a `Flow` or `Repeater` in a `Flickable`
  (loses the whole-row snapping `PageButtons` already provides); letting tiles take variable heights to
  fit the image (breaks whole-page paging, FR-014).

## R-6. Remembering the layout (FR-012)

- **Findings**: persisted preferences are rows in the `settings` table, defaults in `setting_defaults()`,
  validation in `valid_setting()`, exposed through bridge properties (`syncAfterSave`, spec 008).
  `forget_account` leaves settings alone.
- **Decision**: key `picker_view`, values `list`/`tile`, default `list`; property
  `AttachmentsBridge::pickerView` (QString, read/write, `pickerViewChanged`), which reaches the store
  through `NotesBridge::store()`; the page reads it when it is created and writes it on every toggle.
  Setting the current value does nothing; an invalid value is refused by the store and leaves the
  property unchanged.
- **Alternatives considered**: `QSettings` or a file next to the database (a second persistence mechanism);
  keeping it in `NotesBridge` (the picker belongs to `AttachmentsBridge`).

## R-7. The toggle icon and the tile glyphs (FR-008, FR-018)

- **Findings**: `InkButton` shows text only; `res/icons` holds the app icon assets. There is no icon
  font. Gray-level artwork is risky on the panel, so existing screens use bordered text buttons.
- **Decision**: the toggle is a bordered button (same look and size as `InkButton`) whose content is a
  small drawing made of `Rectangle`s: a 2 x 2 grid of squares while the list is shown ("switch to
  tiles") and three horizontal bars while tiles are shown ("switch to list"). The folder glyph is a
  tab plus body; the file glyph is a bordered page with the extension in capitals (at most four characters)
  inside. No asset files, nothing to scale.
- **Alternatives considered**: PNG/SVG assets (new files in the resource list, scaling issues on the 1.3
  pixel ratio); text labels "List" / "Tiles" (the spec asks for an icon, and a text button on the top row
  competes with Cancel and Up).

## R-8. Preventing a double action from two quick taps (spec edge case)

- **Findings**: opening a folder replaces the rows synchronously; a second tap that arrives right after hits
  whatever tile is now under the finger.
- **Decision**: the page ignores taps on entries for 400 ms after it changed folder (a timestamp compared
  in `open(entry)`); Up, Cancel, the toggle and the page buttons are not delayed. The QML test taps twice in
  quick succession and checks that only the first folder was entered.
- **Alternatives considered**: disabling the view during the change (no visible effect on e-ink, the
  disabled state is never drawn in time); doing nothing (the stated edge case would fail).

## R-9. What only the device can tell (walkthrough, not a blocker)

- How the grayscale previews look on the panel, with the chosen tile size and font (R-5 numbers).
- Preview timing for a real photo folder (SC-005) with the decoder pool at one thread.
- Which image formats the device Qt decodes (`image.formats` is already logged at start; webp may fall
  back to the file glyph, which the spec allows).
- Whether `/mnt/ext1` carries all seven folders by their exact names (folder "My pictures" as
  clarified), and that case differences are handled.
- Orientation changes while the browser is open: the grid re-flows because `columns` is bound to the width.
