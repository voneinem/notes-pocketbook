# Implementation Plan: PocketBook Notes MVP

**Branch**: `001-pocketbook-notes-mvp` | **Date**: 2026-10-10 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/001-pocketbook-notes-mvp/spec.md`

## Summary

A Nextcloud Notes client for the PocketBook InkPad One. All sync, API and storage logic lives in a
Qt-free C++17 core library that is unit-tested on the desktop host with a mocked HTTP layer. A thin
Qt bridge exposes it to a single-window QML UI that follows InkFeed's e-ink patterns (no
transitions, flat high-contrast controls, rotation and devicePixelRatio compensation, system
keyboard). The client speaks Nextcloud Notes API v1 over verified HTTPS with an app password,
implements push-then-pull sync, and detects conflicts with `If-Match` (HTTP 412), keeping both
versions. The build follows InkFeed's `TARGET_TYPE=ARM` / host split with manual moc/rcc, but takes
the SDK location only from `POCKETBOOK_SDK` / `-DSDK_PATH`. Packaging is a zip with a launcher
script, the ELF installed as `<app>.bin`, and icons. Working name for `<app>`: `pbnotes`
(placeholder, easy to rename; see research R-14).

## Technical Context

**Language/Version**: C++17 (core, bridge, app); QML (Qt Quick 6.8); POSIX `sh` (launcher,
packaging scripts); C (vendored SQLite amalgamation)

**Primary Dependencies**: Qt 6.8.2 (Core, Gui, Qml, Quick; Concurrent optional) for the bridge and
UI only, the core library uses no Qt; libcurl 7.85 or later (needed for the protocol restriction
options; the SDK ships 8.16.0, the host uses its system package); SQLite (vendored amalgamation,
pinned version); OpenSSL 3 (via libcurl); InkView (device glue only)

**Storage**: SQLite file `/mnt/ext1/system/pbnotes/pbnotes.db` on device (vfat, so
`journal_mode=TRUNCATE`); `./pbnotes-data/` on host. One account row, notes table, settings table.
Password is stored in the DB in clear text (documented, see risks)

**Testing**: Host unit and contract/integration tests of the core library (a minimal in-repo test
runner or a single-header framework chosen in research R-12) against a mocked `HttpTransport`;
recorded, anonymized JSON fixtures of Notes API v1; a fake in-process HTTP server for sync
scenarios; QML/bridge verified manually on host (offscreen) and on device via the quickstart

**Target Platform**: PocketBook InkPad One (RK3566, 32-bit ARM hard-float userland, Qt 6.8.x
runtime on the device) built with the PocketBook SDK; Linux x86-64 host for development and tests

**Project Type**: desktop-style single application (device app) with a host-testable core library

**Performance Goals**: open a note within 3 s with 1,000 notes (SC-003); search 1,000 notes within
2 s (SC-008); sync of 1,000 notes with few changes within 30 s on home Wi-Fi (SC-009); no
animation, minimal repaints

**Constraints**: offline-first (all local operations without network); no TLS verification
bypass; HTTPS only; SDK path only via `POCKETBOOK_SDK` / `-DSDK_PATH`; one sync at a time; DB
writes transactional so interrupted syncs lose nothing; vfat storage (no file permissions, weak
locking); SDK `rcc` unusable (host `rcc` with `--no-zstd`)

**Scale/Scope**: single account; up to ~1,000 notes of typical text size (a few KB each); 6 screens
(setup, list, reader, editor, category/view picker, settings/status)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate (constitution) | Pre-research | Post-design | Evidence |
|---------------------|--------------|-------------|----------|
| I. Test-First: tests written and failing before implementation | PASS | PASS | Core is host-testable; `tasks.md` must order each test task before its implementation task; quickstart lists the test commands |
| II. Simplicity: no speculative abstractions; added complexity justified | PASS with justified items | PASS with justified items | Two items in Complexity Tracking (vendored SQLite, manual moc/rcc). Self-signed trust option and the GitHub Actions workflow are deliberately not built |
| III. Security & Privacy: TLS verification always on; device CA store else bundled CA file; validated input; no secrets in repo | PASS | PASS | HTTPS-only URLs; `VERIFYPEER=1`/`VERIFYHOST=2`; CA selection (device store, else bundled) plus optional user-supplied `custom-ca.pem`, research R-6; server JSON parsed defensively; logs exclude note content and password; password stored unencrypted on device is documented as a residual risk (the constitution requires collecting only needed data and keeping it off the repository, which this satisfies) |
| IV. Integration tests at device boundaries (storage, sync, API formats) | PASS | PASS | Contract tests against API fixtures; store and sync integration tests with SQLite and a fake server; PocketBook-side boundary is the SQLite file layout and launcher/registration (checked in quickstart) |
| V. Host-testable core: sync/API/storage without Qt UI or device; host build target | PASS | PASS | `notes_core` library has no Qt or InkView dependency; `TARGET_TYPE` host build produces `notes_core_tests` |
| VI. No silent data loss: If-Match, 412 → both versions kept | PASS | PASS | Sync algorithm (research R-8) and data model state machine; `PUT` sends `If-Match`; `DELETE` is guarded by a conditional `GET` (or `If-Match` if the server enforces it), residual race recorded in Complexity Tracking; conflict copy uploaded; local changes cleared only after server confirmation |
| Stack: C++17, Qt 6.8 Quick/QML, CMake, libcurl, SQLite | PASS | PASS | Technical Context |
| SDK location only from `POCKETBOOK_SDK` / `-DSDK_PATH` | PASS | PASS | CMake fails with a clear error if neither is set for `TARGET_TYPE=ARM`; no default path, unlike InkFeed |
| Both device and host build targets | PASS | PASS | Project Structure; quickstart |
| Licensing: InkFeed (MIT) patterns attributed in `NOTICE`; notes-android reference-only; license chosen before first public push | PASS | PASS | Project license is MIT (`LICENSE` present); `NOTICE` created in the first implementation task; no notes-android code or text used (behavior from `docs/notes-android-features.md` only); README license statement is task T098 and must precede the first public push |

No violations that block planning. The in-app "trust self-signed certificate" option mentioned in
the plan input is **excluded** from this release (spec FR-003). Private CAs and self-signed server
certificates are supported instead by a user-supplied `custom-ca.pem` (spec FR-038, constitution
Principle III): verification always stays on and no option trusts unverifiable certificates.

## Project Structure

### Documentation (this feature)

```text
specs/001-pocketbook-notes-mvp/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
│   ├── notes-api-v1.md      # Server API subset consumed, headers, status codes
│   ├── core-library.md      # Public interfaces of the Qt-free core
│   └── qml-bridge.md        # Properties/invokables/signals the QML UI relies on
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
CMakeLists.txt                 # top level: TARGET_TYPE=ARM|host, SDK wiring, targets
cmake/
├── sdk.cmake                  # reads POCKETBOOK_SDK / SDK_PATH, sets toolchain (ARM only)
└── qt_manual.cmake            # manual moc/rcc helpers (host rcc --no-zstd, SDK moc)
core/                          # target notes_core (static lib, no Qt, no InkView)
├── include/pbnotes/
│   ├── http_transport.h       # abstract HTTP interface (mockable)
│   ├── curl_transport.h       # libcurl implementation
│   ├── api_client.h           # Notes API v1 client (capabilities, list, get, create, update, delete)
│   ├── note_store.h           # SQLite store
│   ├── sync_engine.h          # push-then-pull, conflicts, status
│   ├── markdown.h             # block/span parser + task-item toggle (Qt-free)
│   ├── title.h                # title derivation from first non-empty line
│   ├── ca_selector.h          # chooses CA file (device store, else bundled) + custom-ca.pem
│   └── types.h                # Note, Account, SyncResult, ...
└── src/*.cpp
third_party/
├── sqlite/sqlite3.c, sqlite3.h   # pinned amalgamation (public domain)
└── cacert/cacert.pem             # pinned CA bundle (MPL-2.0, attributed in NOTICE)
bridge/                        # Qt-dependent glue (moc'd)
├── include/notes_bridge.h, note_list_model.h, markdown_model.h
└── src/*.cpp
app/
└── main.cpp                   # QGuiApplication, data dir, rotation poll, InkView glue (device only)
qml/
├── Main.qml                   # ApplicationWindow + StackView (no transitions), rotated stage
├── InkButton.qml              # flat button (pattern from InkFeed)
└── pages/*.qml                # Setup, NoteList, Reader, Editor, Settings (only if Main.qml grows too large)
res/
├── qml.qrc, app.qrc, icons/   # opaque BMP icons (122x122, 8bpp), one qrc per manual rcc call
platform/pocketbook/
└── pbnotes.app                # launcher shell script (installed as the .app, ELF is pbnotes.bin)
scripts/
├── package-device.sh          # builds the install zip
└── probe-device.sh            # run on the device: CA store, Qt version, Wi-Fi API (research R-6/R-2)
tests/
├── unit/                      # title, markdown, store, ca_selector, sync pieces
├── contract/                  # API client against fixtures
├── integration/               # sync engine + store + fake server
└── fixtures/                  # anonymized API v1 JSON, error bodies
docs/                          # analyses (existing) + install guide
NOTICE                         # InkFeed (MIT) and cacert.pem (MPL-2.0) attributions
README.md                      # build, install (view.json registration), password-storage note
```

**Structure Decision**: single repository with four layers: `core/` (portable, tested on host),
`bridge/` (Qt glue), `qml/` (UI), `app/` (entry point), plus `platform/pocketbook/` and `scripts/`
for device packaging. `TARGET_TYPE=ARM` builds `notes_core`, `bridge`, and the `pbnotes` ELF
(output `pbnotes.app` as in InkFeed, installed as `pbnotes.bin`). The host build builds
`notes_core`, the test executables, and optionally the full app for desktop use.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Vendored SQLite amalgamation instead of the SDK's `libsqlite3` | Host tests and the device must run the same SQLite version and compile options; avoids depending on a distro `libsqlite3` for the host build; stack decision given in the plan input | Using the SDK's shared 3.48.0 only on device and the system library on host risks behavior drift between test and device. Cost is small (one file, pinned version, public domain) |
| DELETE uses a conditional `GET` followed by `DELETE` (two requests) instead of a single `DELETE` with `If-Match` | The server may not honor `If-Match` on `DELETE` (verified by the T022 probe); constitution Principle VI requires the conditional check. Residual risk: a note changed between the `GET` and the `DELETE` is deleted anyway | An unconditional `DELETE` could remove a note edited elsewhere. Mitigation for the residual race: the Nextcloud server's trash bin (where enabled) keeps deleted notes recoverable; if the probe shows a stale `If-Match` on `DELETE` returns 412, the header is sent and this fallback is dropped |
| Manual moc/rcc steps and a hand-written launcher instead of `qt_add_executable` / AUTOMOC | The SDK Qt cannot run its own `rcc` (needs ICU 76); cross-building with CMake's Qt helpers was never verified against this SDK (`docs/pocketbook-sdk-analysis.md` §3); InkFeed proves the manual approach works | `AUTOMOC`/`qt_add_*` would need a working host Qt toolchain config for the SDK; the manual pattern is ~30 lines and already proven on this device class |
