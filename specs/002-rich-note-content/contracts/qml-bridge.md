# Contract: QML bridge additions

Extends `specs/001-pocketbook-notes-mvp/contracts/qml-bridge.md`. `bridge` is the existing
`NotesBridge` context property, `device` the existing `DeviceBridge`, `attachments` a new
`AttachmentsBridge` context property (keeps `notes_bridge.cpp` from growing further).

## Reader block model (`bridge.openNote(localId).blocks[]`)

Existing blocks keep their fields and gain `links`. New kinds are `tablerow` and `attachment`.

| Field | Kinds | Meaning |
|-------|-------|---------|
| `kind` | all | `heading`, `paragraph`, `listitem`, `blank`, `tablerow`, `attachment` |
| `html` | text kinds | Escaped `Text.StyledText` string; links appear as `<a href="l<N>"><u>…</u></a>`; emphasis as before |
| `links` | text kinds, cells | `[{url, confirm}]`; `href="l<N>"` indexes this list. The raw address never appears in markup |
| `unsplittable` | `tablerow`, `attachment` | page breaks must not fall inside the block |
| `header` | `tablerow` | true for the first row of a table |
| `cols` | `tablerow` | `[{html, links, align}]` per column, `align` = `left`/`center`/`right` |
| `weights` | `tablerow` | per-column relative widths (same for every row of the table) |
| `lastRow` | `tablerow` | true on the table's last row (the UI draws the closing rule) |
| `name`, `attKind` | `attachment` | display name; `image` or `file` |
| `attState` | `attachment` | `placeholder` (remote, not downloaded), `ready` (downloaded), `pending` (added here, not uploaded yet), `error` |
| `size` | `attachment` | bytes, `-1` = unknown |
| `errorText` | `attachment` | plain sentence when `attState` is `error` or the last attempt failed, else empty |
| `attachmentId` | `attachment` | row id, 0 if no row exists yet |
| `remotePath` | `attachment` | validated path for download (empty for local tokens) |
| `imageSource` | `attachment` | `image://attachment/<id>` for a ready image, else empty |
| `viewable` | `attachment` | a ready file whose type the reader application can open |

Image requests: `image://attachment/<id>?w=<pixels>` returns an 8-bit grayscale image no wider than
`w`; `w` is the note width for in-note images and the window size for the full-screen page. An
undecodable file yields a null image, and the delegate then shows the placeholder with the reason.

## AttachmentsBridge (new)

| Member | Description |
|--------|-------------|
| `stageAttachment(path)` | Copies the file into the app folder as a staged attachment (≤ 10 MiB) on the worker thread; the call returns at once and the result arrives as `stageFinished({ok, id, token, name, size})` or `{ok:false, errorKey, errorText}`. The editor inserts `token` at the cursor |
| `discardStaged()` | Called when the editor is left (saved or not); drops the staged rows no saved note names (the files are removed on the worker) |
| `listDirectory(path)` → `[{name, dir, size}]` | Directories first, then files, case-insensitive order; hidden entries and unreadable entries skipped; empty list for unreadable paths. `rootDirectory` property gives the start folder |
| `download(noteId, remotePath)` | Starts a download on a worker thread; signals `attachmentsChanged(noteId)` or `attachmentFailed(noteId, remotePath, text)` |
| `probeSizes(noteId)` | If Wi-Fi is up and the server supports attachments, fills unknown sizes (at most 20 per call) with short `HEAD` requests (worker thread); signals `attachmentsChanged(noteId)` |
| `openAttachment(attachmentId)` → bool | Hands a ready file to the system viewer (`device.openFile`, wired by `main.cpp`) after the allow-list check; false (with `attachmentFailed`) if no viewer |
| `supported` property | True when the stored API versions include `1.4`; the editor hides/disables Add otherwise with a message |

`NotesBridge.saveNote` needs no extra call: the store binds the staged attachments named by tokens in
the saved text inside the save transaction; `loadForEdit` returns the text as stored (tokens included, visible as `![name](pbatt:7)`).
`syncNow` results gain the attachment counts and message key `attachments_failed`
("Some attachments could not be uploaded.").

`AttachmentsBridge::Config` carries `rootDirectory`, `openFile(path)` and `wifiConnected()` callbacks, so the
bridge does not depend on the app's `DeviceBridge`. `NotesBridge` also exposes `formatSize(bytes)`
("1.5 KB", "1.0 MB", "" when unknown), `pageTarget(...)`, `effectiveCaPath()`, `attachmentsDir()` and
`refreshTrust()`.

## DeviceBridge additions

| Member | Description |
|--------|-------------|
| `openUrl(url)` → bool | Re-validates `http`/`https`, no whitespace/control characters, ≤ 2,048 characters. Device: `OpenBook(BROWSER_FOR_AUTH, url, 0)`; host: `QDesktopServices::openUrl`. False when rejected or no browser; the UI then shows "The link could not be opened." |
| `openFile(path)` → bool | Only a regular file inside the attachment folder (`setAttachmentsDirectory`, canonical path) and only for the extension allow-list (research R-8). False shows "No viewer for this file on this device." |

No signals; the call returns once the system took over. The app stays on the same page.

## Link activation (reader and table cells)

`Text { textFormat: Text.StyledText; onLinkActivated: page.followLink(blockLinks, link) }`, where
`followLink` parses `l<N>`, looks up `{url, confirm}`, and then either calls `device.openUrl(url)`
at once (`confirm == false`) or opens the existing `ConfirmDialog` with the full address as its
text and "Open" / "Cancel" buttons (`confirm == true`); "Open" calls `device.openUrl(url)`.

## Reader paging

`PageButtons` gets `property var unsplittable: []` — `[{top, bottom}]` in content coordinates of
the blocks that must not be cut. When the list is non-empty `pageBy(direction)` asks
`bridge.pageTarget(contentY, viewHeight, contentHeight, step, direction, unsplittable)`, which wraps
the pure `page_target` of `paging.h` (host-tested; rule in core-library.md). An empty list keeps the
existing code path, so list pages are unchanged. The edge tap zones call the same `pageBy`.

## Editor additions

`EditorPage` gets an "Attach" button (disabled with a note when `!attachments.supported`) that
pushes `FilePickerPage.qml`; the chosen file is staged (`stageAttachment`), then
`area.insert(area.cursorPosition, token + "\n")` (at line start if the cursor is mid-line, matching
the web editor). Leaving with Discard calls `attachments.discardStaged()`. `dirty` includes the
inserted token, so the existing save/discard prompt applies unchanged.

## New pages

- `FilePickerPage.qml` — path line, Up control, list of `listDirectory(path)` entries (large rows,
  paged with `PageButtons`), a confirm dialog with name and size for a tapped file; refuses files
  over the limit with a message before the confirm.
- `ImagePage.qml` — one image fitted to the whole window (`image://attachment/<id>?w=<window
  width>`, `fillMode: Image.PreserveAspectFit`, asynchronous), a Back control top-left, no zoom or
  pan.
