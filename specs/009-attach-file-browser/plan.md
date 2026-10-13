# Implementation Plan: Attach File Browser

**Branch**: `feature/009-attach-file-browser` | **Date**: 2026-10-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/009-attach-file-browser/spec.md`

## Summary

The file browser opened by "Attach" in the editor (`FilePickerPage.qml`, feature 002) changes in three
ways: its top level shows only seven named folders, it can show the current folder as a list or as a
grid of tiles, and in the tile view image files show a small preview. Nothing about attaching changes
(size limit, confirmation, copy on attach). Built on the existing layers (Qt-free core, thin Qt bridge,
QML) with **no core change, no new library, no schema change and no server change**:

- **The folder rule lives in the bridge.** `AttachmentsBridge::listDirectory(path)` applies the rule
  when `path` is the browser's root (`Config::rootDirectory`): it returns only the directories whose
  name equals (case-insensitively, whole name) one of `Config::topFolders`, set by `app/main.cpp` to
  the seven names. Any other path is listed as today. "Up" never goes above the root because the page
  disables it there. Existing tests keep passing because an empty `topFolders` means "no filter" (the
  host tests set it explicitly for the new cases).
- **Entries get two new fields.** `image` (the extension is one `attachment_kind_for` calls an image,
  the same rule as for attachments in notes) and `previewId` (see next point), both only on regular,
  readable files.
- **Previews arrive through a small image provider**, `image://filepreview/<previewId>`. The bridge
  keeps a *preview index*: every `listDirectory` call replaces it with `previewId → absolute path` for
  the image entries just listed (ids from a counter that never repeats). The provider can therefore
  read only files the browser itself listed, and no file name ever goes through a URL. It decodes on
  its **own single-thread pool**, scaled to the requested width as 8-bit grayscale by the existing
  `load_attachment_image` (pixel-count guard, EXIF, scaled decoding), keeps the results in a small
  **byte-bounded cache** (8 MB) keyed by path, size and modification time, and honors `cancel()` so
  tiles that leave the page cost nothing.
- **The layout choice is one setting row** `picker_view` (`list`/`tile`, default `list`) in the existing
  key/value settings table, exposed as the property `AttachmentsBridge::pickerView`.
- **QML**: `FilePickerPage.qml` gets a toggle button at the top (glyph drawn from rectangles, no image
  asset), a `GridView` next to the existing `ListView` (only one is active), tile delegates with a
  drawn folder / file glyph or the preview, and the root-aware "Up". `PageButtons` is reused unchanged
  (`snap` = the cell height), so paging stays by whole rows of tiles.

Research resolved every unknown (R-1 to R-9). The device walkthrough covers what the host cannot
show: how the grayscale previews look on the panel, how many tiles fit, and preview timing.

## Technical Context

**Language/Version**: C++17 (bridge, app); QML (Qt Quick 6.8) — unchanged.

**Primary Dependencies**: unchanged. No new library; `QImageReader`, `QQuickAsyncImageProvider`,
`QThreadPool` and `QCache` are Qt classes already in use or in modules already linked.

**Storage**: one new row in the existing `settings` table (`picker_view`), default `list`, validated by
the store like the other settings. No schema version change, no migration. Previews are held in memory
only and never written to the device (FR-022).

**Testing**: the existing harness — host unit tests (CTest, `tests/unit/test_*.cpp`), offscreen Qt bridge
tests (`tests/bridge`), QML page tests with injected mouse events (`tests/qml`, stubs in `stubs.qml`),
app `--self-test`, manual device walkthrough. New: `test_file_preview_provider` (bridge),
`file_picker.qml` (QML). Extended: `test_attachments_bridge` (top level, entry fields, setting),
`test_store_open` (the setting), `stubs.qml` (the `attachments` stub gains the picker members).

**Target Platform**: PocketBook InkPad One (Qt 6.8.x runtime, SDK build) and Linux x86-64 host — unchanged.

**Project Type**: single application with a host-testable core library — unchanged.

**Performance Goals**: toggling the layout under 2 s on the device (SC-004): it swaps which view is
visible over the same already-listed rows, no re-listing. 90% of the previews of a page within 10 s
(SC-005): a page is at most ~30 tiles; scaled JPEG/PNG decoding of an ordinary photo is tens of
milliseconds on the device's CPU, one decoder thread leaves the other core to the UI. Taps, paging and
folder changes never wait for a preview (FR-020).

**Constraints**: preview memory bounded (8 MB of decoded previews, SC-006, FR-021); no file or folder
names in logs (FR-025); read-only browsing (FR-024); e-ink: no animation, grayscale previews, whole-page
paging (FR-014); orientation changes only change the number of tile columns (bindings on the width).

**Scale/Scope**: 1 new bridge class (`FilePreviewProvider`, ≈120 lines) + a small preview index inside
`AttachmentsBridge`; `listDirectory` ≈ 25 changed lines; 1 setting; `FilePickerPage.qml` grows by about
150 lines (tile delegate, toggle, root-aware Up); about 1 new bridge test file, 1 new QML test and 3
existing tests extended.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate (constitution) | Pre-research | Post-design | Evidence |
|---------------------|--------------|-------------|----------|
| I. Test-First | PASS | PASS | Every rule is a testable function of a directory fixture or a request: the top-level filter, the entry fields, the setting, the provider (scaling, grayscale, refusals, cache bound, cancel) and the page behavior (Up disabled at the top, toggle keeps folder, tile taps). `tasks.md` must order each failing test before its code |
| II. Simplicity (YAGNI) | PASS with justified items | PASS with justified items | Two items in Complexity Tracking. Rejected: a Settings screen for the folder names, thumbnails stored on disk, sorting or searching in the browser, multi-select, folder-size or date columns, a third layout |
| III. Security & Privacy | PASS | PASS | Nothing is sent anywhere. The provider can read only files the browser listed (registry), only regular readable files with an image extension, within the existing 40 million pixel guard; browsing is read-only; log lines carry counts and codes only, never names (FR-025) |
| IV. Integration tests at device boundaries | PASS | PASS | No boundary format changes. The listing is exercised against real temporary directories (folders, files, links, hidden and unreadable entries) and the provider against real image files in `tests/bridge`, with anonymized generated fixtures |
| V. Host-testable core | PASS | PASS | No core change. All new logic is in the bridge (listing, registry, provider) and QML and runs offscreen on the host without InkView; the bridge already depends on Qt by design |
| VI. No silent data loss | PASS | PASS | Browsing is read-only; attaching still copies the original and leaves it untouched (FR-007, FR-022); previews are never attached |
| Stack: C++17, Qt 6.8 Quick/QML, CMake, libcurl, SQLite | PASS | PASS | No stack change |
| SDK location only from `POCKETBOOK_SDK` / `-DSDK_PATH` | PASS | PASS | One new source in the existing bridge source list |
| Both device and host build targets | PASS | PASS | New code is in existing targets |
| Licensing: InkFeed attributed; notes-android reference-only | PASS | PASS | Nothing copied; behavior written from the spec |
| Release version | PASS | PASS | A MINOR bump to 1.5.0 with a CHANGELOG entry is the last task of the feature |

No violations block planning. The seven folder names are device-layout facts (like the root
`/mnt/ext1`) and are supplied by `app/main.cpp`, not typed into the bridge.

## Project Structure

### Documentation (this feature)

```text
specs/009-attach-file-browser/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R-1 .. R-9
├── data-model.md        # Phase 1: top level, entry, layout choice, preview index and cache
├── quickstart.md        # Phase 1: host tests and device walkthrough
├── contracts/
│   └── qml-bridge.md    # AttachmentsBridge additions, the filepreview image source, the page's behavior
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks — not created here)
```

No `contracts/core-library.md`: the core library is not touched.

### Source Code (repository root)

```text
core/src/note_store.cpp         # + picker_view in setting_defaults / valid_setting
core/include/pbnotes/note_store.h   # settings doc: picker_view {list,tile}
bridge/
├── include/
│   ├── attachments_bridge.h    # + Config::topFolders, pickerView property, previewIndex(); listDirectory doc
│   └── file_preview_provider.h # new: PreviewIndex, FilePreviewProvider (async image provider "filepreview")
└── src/
    ├── attachments_bridge.cpp  # top-level filter, entry fields image / previewId, pickerView
    └── file_preview_provider.cpp   # new
bridge/CMakeLists.txt           # + src/file_preview_provider.cpp (moc: header)
app/main.cpp                    # Config::topFolders; addImageProvider("filepreview", ...)
qml/pages/FilePickerPage.qml    # toggle, GridView + tile delegate, root-aware Up, tap guard
tests/
├── bridge/                     # test_file_preview_provider (new); test_attachments_bridge (+ top level, fields, pickerView)
├── unit/                       # test_store_open (+ picker_view default / validation / persistence)
├── qml/                        # file_picker.qml (new), stubs.qml (+ attachments members)
└── CMakeLists.txt              # + test_file_preview_provider, qml_file_picker
VERSION, CHANGELOG.md           # 1.5.0 (last task)
```

**Structure Decision**: the repository's existing layout is kept; no new module or library. Rules that
decide *what is listed* sit in the bridge next to the existing listing (the only place that touches the
file system for the picker); the provider is a sibling of `AttachmentImageProvider`; QML only lays out
what the bridge returns.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| C-1. New class `FilePreviewProvider` with a preview index and a decoded-image cache (≈120 lines) | FR-017..FR-022 and SC-005/006 need off-thread, cancellable, memory-bounded previews of arbitrary files in arbitrary folders, readable only if the browser listed them | A QML `Image { source: "file://" + path }` decodes full size on the UI thread (a 12 MP photo freezes the page and uses ~50 MB per tile), cannot be capped, cannot be cancelled and would let any QML string read any file. Reusing `AttachmentImageProvider` does not fit: it resolves database attachment ids, not file paths |
| C-2. A new persisted setting `picker_view` | FR-012 asks the layout to survive leaving the browser and restarting the app | Holding the choice in QML or the bridge object loses it at restart; a file of its own would bypass the validated settings table the app already uses |
