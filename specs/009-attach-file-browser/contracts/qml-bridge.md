# Contract: QML bridge and file browser page

Extends `specs/002-rich-note-content/contracts/qml-bridge.md` and
`specs/008-autosync-on-save/contracts/qml-bridge.md`. No new context property; the core library
contract is unchanged.

## `AttachmentsBridge` additions

| Member | Behavior |
|--------|----------|
| `Config::topFolders` (`QStringList`) | names of the folders listed at the root; empty = the root is listed unfiltered. Set by `app/main.cpp` |
| `listDirectory(path)` (changed) | at the root (`path` equals `rootDirectory` after `QDir::cleanPath`): only directories whose name equals a `topFolders` item, ignoring case and as a whole name; elsewhere as before. Every entry map gains `image` (bool) and `previewId` (int, 0 unless `image`). Replaces the preview index with the ids of this listing. Still: folders first, case-insensitive name order, hidden / unreadable / dangling skipped, empty for an unreadable or empty path |
| `pickerView` (property, QString `"list"` or `"tile"`, read/write, `pickerViewChanged`) | reads/writes the persisted setting `picker_view`; writing the current value does nothing; an invalid value is ignored. Works without an account |
| `previewIndex()` (C++ only) | the shared index handed to `FilePreviewProvider` in `app/main.cpp` |

`rootDirectory`, `maxBytes`, `stageAttachment`, `discardStaged` and the other members are unchanged.

## Image source `image://filepreview/<previewId>`

| Aspect | Contract |
|--------|----------|
| Input | the integer `previewId` of an entry of the **latest** `listDirectory`; the requested width is the QML `sourceSize.width` |
| Output | an 8-bit grayscale image no wider than the request, never enlarged, EXIF orientation applied, proportions kept |
| Null image (QML status `Error`) | unknown or outdated id; the file is gone, unreadable or not a regular file; the format cannot be decoded; the file is described as more than 40 million pixels (refused before decoding) |
| Threading | decoded on a private single-thread pool; the response completes asynchronously; a cancelled response never decodes if it had not started |
| Memory | decoded results are cached up to 8 MB in total, least recently used dropped first; nothing is written to disk |
| Side effects | none on the file; no network; no log line contains a path or name |

## `FilePickerPage.qml`

**Top row**: `Cancel`, `Up`, and the layout toggle (right-aligned in the row, same height as the other
buttons).

| Element | Behavior |
|---------|----------|
| `Up` | disabled when `path` equals `attachments.rootDirectory` (normalized); otherwise goes to the parent, clears the message, shows the first page |
| Toggle | draws a 2 x 2 square grid while the list is shown and three bars while tiles are shown; a tap flips `view`, writes `attachments.pickerView`, keeps `path`, `message` and the rows, and shows the first page |
| Path line | at the root shows the root path as today |
| List view | unchanged: 104 px rows with kind label, name and size; `PageButtons { snap: 104 }`; no previews and no `previewId` use |
| Tile view | `GridView`, `columns = max(2, floor(width / 270))`, `cellHeight = 250`; `interactive: false`, `cacheBuffer: 0`; `PageButtons { snap: cellHeight }` |
| Tile content | image entries: `Image` from `image://filepreview/<previewId>` over the file glyph (the glyph stays while the status is not `Ready`); folders: folder glyph; other files: file glyph with the extension; below: the name, at most two lines, elided |
| Tap on a folder (either view) | enters it, clears the message, shows the first page |
| Tap on a file (either view) | as today: over `attachments.maxBytes` → the size message; otherwise the confirmation "Attach this file?" with name and size; accept emits `picked(path)` and pops |
| Empty states | at the root: "Nothing to browse here. The folders Audio Books, Downloads, ... were not found." (the names come from the same list the app uses; text kept short); in a folder that is empty or unreadable: the existing "This folder is empty or cannot be read." |
| Tap guard | taps on entries within 400 ms after a folder change are ignored |
| Start | always at `rootDirectory`; `view` from `attachments.pickerView` |

## Test stubs (`tests/qml/stubs.qml`)

The `attachments` stub gains `rootDirectory` (string), `maxBytes` (number), `pickerView` (string,
writable), `listDirectory(path)` returning a fixture (root: three of the seven folders; two folders
below with a mix of images, a document and a subfolder, enough entries for several pages) and a
`formatSize` equivalent where the page uses `bridge.formatSize`. `file_picker.qml` asserts: the root
lists only folders and `Up` is disabled there; opening a folder and `Up` returns to the root; the
toggle switches the view without leaving the folder and writes `pickerView`; a new page starts in the
stored view; folder and file taps work in both views and the confirmation appears for a file; paging
moves by whole rows; the size message appears for an oversized file; two quick taps enter one folder.

## Log events

None. The provider and the listing log nothing, so no log line can contain a path or a file name (FR-025). A preview
that cannot be decoded shows the file symbol; the engine's own warning names only the numeric preview id.
