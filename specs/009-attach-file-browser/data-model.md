# Data Model: Attach File Browser

**One setting row is added; no table, column, file or sync field changes.** The schema stays at the
version of feature 008. Notes, attachments, the pending-change queue and the sync protocol are untouched.
Nothing is written to the device's storage by browsing (FR-024).

## Stored: the layout choice (`settings` table, existing key/value rows)

| Key | Values | Default | Notes |
|-----|--------|---------|-------|
| `picker_view` | `list`, `tile` | `list` (returned when the row is absent) | Any other value is refused by the store. Not touched by `forget_account`. Read when the browser page is created, written on every toggle (FR-012) |

Upgrading from any earlier version needs no migration: the row does not exist and reads as `list`.

## Configuration: the browsable top level

| Item | Where | Value |
|------|-------|-------|
| `rootDirectory` | `AttachmentsBridge::Config` (existing) | `/mnt/ext1` on the device, the home folder on the host |
| `topFolders` | `AttachmentsBridge::Config` (new) | `Audio Books`, `Downloads`, `Music`, `My pictures`, `Notes`, `Photo`, `screens`; empty list = no filter (tests) |

Rule (FR-001..FR-003): at the root, an entry is listed iff it is a directory (a link to a directory
counts) and its name equals a `topFolders` item ignoring case, as a whole. No file is listed at the root.
Hidden, unreadable and dangling entries are skipped at every level, as today.

## In memory: an entry (what `listDirectory` returns per row)

| Field | Type | Meaning |
|-------|------|---------|
| `name` | string | file or folder name (existing) |
| `dir` | bool | folder (existing) |
| `size` | number | bytes for files, 0 for folders (existing) |
| `image` | bool | **new**: regular readable file whose extension `attachment_kind_for` calls an image; false for folders and everything else |
| `previewId` | number | **new**: non-zero only when `image` is true; the id the tile gives to `image://filepreview/` |

Order (FR-013): folders first, then files, each case-insensitively by name — unchanged.

## In memory: the preview index (owned by `AttachmentsBridge`, shared with the provider)

| Field | Meaning |
|-------|---------|
| `generation` | counter that only grows; the next `previewId` comes from it, so an id is never reused |
| `paths` | `previewId → absolute path` for the image entries of the **latest** listing only |

Transitions: `listDirectory` clears `paths` and fills it for the new listing (one mutex-protected
swap). An id from an older listing resolves to nothing. The index holds at most the number of image
entries in one folder.

## In memory: the preview cache (inside the provider)

| Field | Meaning |
|-------|---------|
| key | `path`, file size, modification time and requested width, joined |
| value | the decoded 8-bit grayscale image, no wider than the request |
| bound | total decoded bytes at most 8 MB; the least recently used image is dropped first (FR-021) |

The cache is never written to disk and is dropped with the process. A changed file (size or time)
produces a different key, so a stale preview is not shown for a replaced file.

## Page state (QML, not persisted)

| State | Meaning |
|-------|---------|
| `path` | current folder (string); starts at the root every time the page is created (US1-6) |
| `view` | `list` or `tile`, from `pickerView` at creation, written back on toggle |
| `message` | the existing notice line (size limit, unreadable folder) |
| `lastNavigation` | time of the last folder change, for the 400 ms tap guard |
