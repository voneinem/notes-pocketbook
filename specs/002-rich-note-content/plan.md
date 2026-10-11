# Implementation Plan: Rich Note Content (Links, Tables, Attachments, Emojis)

**Branch**: `002-rich-note-content` | **Date**: 2026-10-10 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/002-rich-note-content/spec.md`

## Summary

Four additions to the reader and editor of the existing app, all built on the MVP architecture
(Qt-free core, thin Qt bridge, QML UI):

- **Links** — the inline Markdown parser keeps link targets and detects bare `http(s)` addresses; a
  pure function decides whether a tap needs confirmation (only when the link text is not the
  address itself); the device hand-off uses the system browser exactly as InkFeed does.
- **Tables** — a small GitHub-style pipe-table parser produces table blocks; the bridge flattens
  them into one reader block per row so column widths are shared, cells wrap, and page breaks
  never cut a row.
- **Emojis** — preserved end to end (tests first: JSON surrogate escapes, store, sync, search),
  drawn through a bundled monochrome emoji font as a per-glyph fallback, and never cut: the excerpt
  builder gets a Qt-free display-unit helper while the UI relies on Qt's grapheme rules.
- **Attachments** — Notes API `v1.4` attachment endpoints; files are downloaded **only on request**
  (placeholder → Download), shown in place (images, grayscale) or opened with the device viewer;
  adding a file from the device stages a local token in the text, uploads before the reference
  reaches the server, and survives interruption; a new `attachments` table (schema 2) and a
  transport that can stream files carry it.

Research resolved every unknown (R-1 to R-15); a handful of device-only facts are probe tasks with
a stated fallback, none blocks the design.

## Technical Context

**Language/Version**: C++17 (core, bridge, app); QML (Qt Quick 6.8); POSIX `sh` (packaging) —
unchanged.

**Primary Dependencies**: unchanged (Qt 6.8.2 Core/Gui/Qml/Quick for bridge and UI only; libcurl ≥
7.85, which already provides `curl_mime` for multipart upload; vendored SQLite; InkView for device
glue only). One new third-party **asset**, no new library: the Noto Emoji monochrome font (SIL OFL
1.1), pinned and attributed (research R-12).

**Storage**: the SQLite file gains an `attachments` table (schema version 2, data-model.md);
attachment files live in `<data dir>/attachments/<id>.<ext>` on the device (`/mnt/ext1/system/
pbnotes/attachments/`) and `./pbnotes-data/attachments/` on the host. vfat caveats as in the MVP.

**Testing**: the MVP harness (host unit/contract/integration tests against a mocked transport and
the fake Notes server, offscreen Qt for the bridge, manual quickstart on the device). New tests are
listed in quickstart §1 and research R-14; the fake server learns the three attachment routes.

**Target Platform**: PocketBook InkPad One (Qt 6.8.x runtime) and Linux x86-64 host — unchanged.

**Project Type**: desktop-style single application with a host-testable core library — unchanged.

**Performance Goals**: a note with a few links, one table, a few emojis and a 1 MB image opens within
3 s in a 1,000-note collection (SC-005); images decode asynchronously and scaled so paging is never
blocked; table parsing is linear in the note size; sync time does not depend on attachments
(downloads are on request, uploads only for notes the user changed).

**Constraints**: offline-first (everything except Download, size probing, and upload works without
a network); TLS verification and credential rules unchanged for the new requests; 10 MiB per
attachment; no file from the server ever supplies a local path; the user's original files are only
read; the app never contacts a link's destination itself; no zoom/pan; no table editor; no emoji
picker; text only inside table cells (no images in cells).

**Scale/Scope**: unchanged collection size (~1,000 notes); tens of attachments per device are the
expected use because nothing is fetched automatically. Two new pages (file picker, full-screen image)
and extended reader/editor.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate (constitution) | Pre-research | Post-design | Evidence |
|---------------------|--------------|-------------|----------|
| I. Test-First | PASS | PASS | Every new core function is pure or fake-transport testable; `tasks.md` must order each test task before its implementation; test list in quickstart §1 |
| II. Simplicity (YAGNI) | PASS with justified items | PASS with justified items | Four items in Complexity Tracking (emoji font asset, local token with staged ordering, transport file I/O, in-app file picker). Rejected as YAGNI: zoom/pan, table editor, emoji picker, emoji coverage detector, HEAD during sync, a decoder library, deleting server files |
| III. Security & Privacy | PASS | PASS | Browser opens only after a user tap and only `http`/`https` after a second validation in `DeviceBridge`; the app never fetches link targets (spec FR-005); attachment requests use the same verified-TLS transport and redirect rules as notes (existing tests extended to the new methods); attachment paths validated before use and never turned into local file names (`<id>.<ext>`, ext restricted); staged originals are only read (Security & Data Handling: no source data modified); logs carry counts and error classes only, no names, paths or addresses; the font asset is pinned (commit + SHA-256) and reviewed |
| IV. Integration tests at device boundaries | PASS | PASS | Contract tests for the three endpoints and their failures; migration test for schema 1→2; sync integration tests for add/upload/interrupt/conflict; the PocketBook-side boundaries (`OpenBook` for browser and viewer, image formats, fonts) are covered by probes and quickstart §3–4 |
| V. Host-testable core | PASS | PASS | Link, table, attachment-reference, cluster, store, transport-contract and sync logic are all in `notes_core` with no Qt/InkView; Qt-only pieces are the image provider, directory listing, and the two `DeviceBridge` calls |
| VI. No silent data loss | PASS | PASS | A reference reaches the server only after its file did (research R-3); the stored text is rewritten transactionally and idempotently; `confirm_push` still clears a local change only when the server confirmed exactly what was sent; a failed upload never drops text or the file; conflicts keep attachments with the copy (R-4); `.part` + rename for every file write; remaining safe duplicates (an upload whose response was lost leaves an unreferenced server file; a token pasted into a second note stays literal text) are recorded in Complexity Tracking |
| Stack: C++17, Qt 6.8 Quick/QML, CMake, libcurl, SQLite | PASS | PASS | No stack change |
| SDK location only from `POCKETBOOK_SDK` / `-DSDK_PATH` | PASS | PASS | No build-system path added; the font is installed by the packaging script from `third_party/` |
| Both device and host build targets | PASS | PASS | New core files build in both; `DeviceBridge` calls are `#ifdef PLATFORM_POCKETBOOK` with a desktop equivalent |
| Licensing: InkFeed attributed; notes-android reference-only; MIT project | PASS | PASS | The browser launch pattern is covered by the existing InkFeed `NOTICE` entry (device glue) and extended by one sentence; the OFL font gets its own `NOTICE` entry and license file shipped with the package; Notes server behavior was read from the public API description and routes only — no code, strings or comments copied; nothing taken from notes-android |

No violations block planning.

## Project Structure

### Documentation (this feature)

```text
specs/002-rich-note-content/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── notes-attachments-api.md   # Server endpoints consumed, limits, failures, fixtures
│   ├── core-library.md            # New and changed Qt-free interfaces
│   └── qml-bridge.md              # Reader block model, AttachmentsBridge, DeviceBridge, pages
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root) — additions and changes only

```text
core/include/pbnotes/
├── utf8.h                    # NEW  display-unit (emoji cluster) helper
├── links.h                   # NEW  is_followable_url, link_needs_confirmation
├── attachment_ref.h          # NEW  reference validation, encoding, tokens, kinds
├── attachment_service.h      # NEW  add (stage), download, probe size, janitor
├── paging.h                  # NEW  page_target: page edges that never cut a row or image
├── markdown.h                # CHG  Span.url/link_confirm, Table and Attachment blocks
├── types.h                   # CHG  Attachment, kinds/states, counts, kMaxAttachmentBytes
├── http_transport.h          # CHG  upload_file, save_body_to, max_body_bytes, HEAD
├── api_client.h              # CHG  upload/download/probe attachment, supports_attachments
├── note_store.h              # CHG  schema 2, attachment operations
├── sync_engine.h             # CHG  attachment-aware push, counts
└── title.h                   # CHG  make_excerpt stops only at display-unit boundaries
core/src/
├── utf8.cpp, links.cpp, attachment_ref.cpp, attachment_service.cpp, paging.cpp  # NEW
├── note_store_attachments.cpp   # NEW  attachment operations of NoteStore (note_store.cpp is 1,000+ lines)
├── sync_attachments.cpp         # NEW  attachment-aware push used by sync_engine.cpp
├── markdown.cpp              # CHG  block parsing (tables, attachment blocks)
├── markdown_inline.cpp       # NEW  split out of markdown.cpp: links, autolinks, emphasis, attachment references
├── markdown_table.cpp        # NEW  table recognition and cell/weight computation
├── markdown_internal.h, store_sql.h, sync_attachments.h   # NEW  private headers shared by the files above
├── curl_transport.cpp        # CHG  curl_mime upload, file sink, size cap, HEAD
├── api_client.cpp, note_store.cpp, sync_engine.cpp, title.cpp, types.cpp    # CHG
bridge/
├── include/attachments_bridge.h, attachment_image_provider.h                # NEW
├── src/attachments_bridge.cpp, attachment_image_provider.cpp                # NEW
├── include/reader_blocks.h, src/reader_blocks.cpp                           # NEW  block model split out of notes_bridge.cpp
└── src/notes_bridge.cpp      # CHG  pageTarget, formatSize, attachment message key
app/
├── device_bridge.h/.cpp      # CHG  openUrl, openFile
└── main.cpp                  # CHG  register provider and context property, load emoji font,
                              #      log supported image formats
qml/
├── pages/ReaderPage.qml      # CHG  link activation, table rows, attachment delegates, unsplittable list
├── pages/EditorPage.qml      # CHG  Attach button, token insertion, discard of staged files
├── pages/FilePickerPage.qml  # NEW
├── pages/ImagePage.qml       # NEW
├── TableRowView.qml, AttachmentView.qml                                     # NEW  reader delegates
├── PageButtons.qml           # CHG  unsplittable-aware paging (calls bridge.pageTarget)
├── BlockText.qml             # NEW  text-block delegate split out of ReaderPage.qml
└── qml.qrc                   # CHG
third_party/noto-emoji/       # NEW  NotoEmoji[wght].ttf (renamed), OFL.txt, VERSION (commit, SHA-256)
platform/pocketbook/pbnotes.app   # CHG only if the font path must be exported (no: loaded by absolute path)
scripts/package-device.sh     # CHG  installs pbnotes-emoji.ttf and its license
scripts/check-constitution.sh # CHG  also flags logging of URLs/paths/attachment names
NOTICE                        # CHG  Noto Emoji (OFL-1.1) entry; InkFeed entry mentions browser launch
INSTALL.md, README.md         # CHG  new capabilities, 10 MiB limit, font license
tests/
├── unit/                     # test_utf8_cluster, test_links, test_markdown_links, test_markdown_table,
│                             # test_markdown_attachments, test_paging, test_attachment_ref,
│                             # test_store_attachments, test_store_attachments_staging,
│                             # test_attachment_service_add, test_store_migration_v2, test_emoji_roundtrip
├── contract/                 # test_api_attachments, test_api_attachment_upload
├── integration/              # test_sync_attachments, test_sync_attachments_push,
│                             # test_attachment_transport, test_attachment_upload_transport
├── bridge/                   # test_reader_blocks, test_attachment_image_provider, test_attachments_bridge
├── support/                  # fake_notes_server.h and fake_transport.h learn the new routes/fields
└── fixtures/                 # attachment_* fixtures (anonymized)
```

**Structure Decision**: no new top-level directories or projects. The Markdown parser is split into
three files because its present 257 lines would otherwise roughly triple; everything attachment-
related in the core sits behind `NoteStore` and `AttachmentService`; the Qt bridge gets one new
class (`AttachmentsBridge`) so `notes_bridge.cpp` (546 lines) does not keep growing.

## Spec amendments made during planning

Reality checks against the Notes API and the data available changed four statements of
`spec.md`; they were edited in place and are listed here for review:

1. FR-019 / edge case: the placeholder shows the size **when known** (the note data carries none;
   research R-2).
2. FR-023 / User Story 4 scenario 5: local copies are removed when the note is deleted, when the
   reference disappears from the text on the server, or when the server reports the file gone — a
   server-side file deletion alone cannot be observed without trying to download it.
3. Edge case "table cell with image": attachments inside table cells are shown by name only.
4. Edge case: an attachment that does not exist on the server is reported when the user tries to
   download it (not during sync, which never touches attachments).

## Risks

| # | Risk | Effect | Mitigation |
|---|------|--------|------------|
| 1 | Device Qt lacks image plugins (JPEG/GIF/WebP) | Some images show as files | Probe logs formats; unsupported ⇒ file entry; add a decoder only if JPEG itself fails |
| 2 | Emoji font fails to load or does not shape ZWJ/flag sequences | Parts shown side by side, or the device font only | Probe; the data and layout rules (never cut, never changed) hold regardless; fallback to the device font |
| 3 | The app does not resume cleanly after the browser | Reading position lost | Probe; persist note id + position before `openUrl` if needed |
| 4 | Servers without `HEAD` length | Sizes stay unknown until download | Designed for (name-only placeholder) |
| 5 | Server rejects empty content on create | Notes made only of attachments cannot be created first | Probe; fall back to one placeholder line replaced in the PUT |
| 6 | Conflict copy in a different category cannot resolve remote-only references | Placeholder with a reason in the copy | Documented limitation (research R-4); the original keeps its attachments |
| 7 | Lost upload response | Unreferenced duplicate file on the server | Same class as the MVP lost-POST case; no text lost |

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Bundled Noto Emoji font (new third-party asset, about 2 MB, OFL-1.1) | The device UI font's emoji coverage is unverified and likely partial; a monochrome emoji font gives deterministic drawing on a monochrome panel (spec FR-014, SC-003) | Relying on the device font alone cannot meet SC-003 if coverage is poor and cannot be fixed afterwards. Kept as the fallback if loading fails. A color font is 5× larger and useless on e-ink |
| Local `pbatt:` token, staged attachments and a create-then-upload-then-PUT order (new concept in the text and a `staged`/`pending` state) | A reference needs the server's file path, the upload needs the server's note id, and no other device may see a reference to a missing file (spec FR-028); offline add (FR-027) means the file exists before the note does | Writing the final path up front is impossible (id and name unknown); creating the note with the reference and uploading later exposes broken references; blocking add until online breaks offline use |
| Transport extended for file upload, file download sink, size cap and HEAD | Notes API attachments are multipart uploads and binary downloads up to 10 MiB; the string-body interface would hold whole files in memory and cannot send files | A second transport duplicates TLS/CA/redirect handling (constitution III risk); in-memory bodies risk memory pressure on the device |
| In-app file picker page | No file chooser exists on the device (spec FR-025) | `Qt.labs.folderlistmodel`/system dialogs are unverified on the reader's Qt runtime; the bridge call plus ~80 lines of QML is smaller than discovering that on device |
