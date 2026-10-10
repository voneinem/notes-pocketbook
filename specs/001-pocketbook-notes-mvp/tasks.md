---

description: "Task list for PocketBook Notes MVP"
---

# Tasks: PocketBook Notes MVP

**Input**: Design documents from `/specs/001-pocketbook-notes-mvp/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: REQUIRED. The constitution makes Test-First non-negotiable (Principle I): every test task
below MUST be written and observed FAILING before the implementation task that follows it. Tests
for `notes_core` run on the host without Qt, the SDK, or a device (Principle V).

**Organization**: Tasks are grouped by user story. Working name `pbnotes` (research R-14). Paths are
relative to the repository root `<projects>/notes-pocketbook`.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US6 as in spec.md
- **[MANUAL]**: needs a person, the device, or a live Nextcloud server; record results in the named
  file. An implementing agent skips these and reports them as open

## Conventions that apply to every task

- C++17; namespace `pbnotes`; `notes_core` MUST NOT include Qt or `inkview.h` headers.
- TLS verification MUST never be disabled; no `CURLOPT_SSL_VERIFYPEER`/`VERIFYHOST` set to 0 anywhere.
- SDK paths MUST NOT be hard-coded; only `-DSDK_PATH` / `POCKETBOOK_SDK`.
- Logs MUST NOT contain note text, passwords, or user names.
- Do not copy code or text from notes-android; InkFeed patterns are adapted and attributed in `NOTICE`.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Repository skeleton, build wiring, vendored third-party files

- [ ] T001 Create the directory skeleton from plan.md: `core/include/pbnotes/`, `core/src/`, `third_party/sqlite/`, `third_party/cacert/`, `bridge/include/`, `bridge/src/`, `app/`, `qml/`, `res/icons/`, `platform/pocketbook/`, `scripts/`, `cmake/`, `tests/unit/`, `tests/contract/`, `tests/integration/`, `tests/fixtures/`, `tests/support/` (add `.gitkeep` where empty)
- [ ] T002 Create top-level `CMakeLists.txt` (min 3.16, C++17, `-DTARGET_TYPE=ARM` vs host, `enable_testing()`, variable `PBNOTES_APP_NAME=pbnotes`, targets `notes_core` static lib, `notes_core_tests` via CTest; ARM branch includes `cmake/sdk.cmake` before `project()`; host build MUST NOT require Qt or the SDK)
- [ ] T003 [P] Create `cmake/sdk.cmake`: `SDK_PATH` from `-DSDK_PATH`, else environment variable `POCKETBOOK_SDK`; if neither is set stop with `message(FATAL_ERROR ...)` naming both options; NO default path. Derive `SYSROOT=${SDK_PATH}/arm-buildroot-linux-gnueabihf/sysroot`, compilers `${SDK_PATH}/bin/arm-buildroot-linux-gnueabihf-{gcc,g++}`, `CMAKE_SYSTEM_NAME Linux`, `CMAKE_SYSTEM_PROCESSOR arm`, `CMAKE_SYSROOT`, `CMAKE_FIND_ROOT_PATH`, find modes PROGRAM=NEVER / LIBRARY=ONLY / INCLUDE=ONLY (pattern adapted from InkFeed `CMakeLists.txt:9-25`)
- [ ] T004 [P] Create `cmake/qt_manual.cmake`: `HOST_MOC` from `${SDK_PATH}/local/qt6/libexec` (NO_DEFAULT_PATH, REQUIRED); `HOST_RCC` searched in `/usr/lib/qt6/libexec`, `/usr/lib/x86_64-linux-gnu/qt6/libexec`, `/usr/lib/qt6/bin` (REQUIRED, error message names package `qt6-base-dev-tools`); print the found rcc version at configure time; functions `pbnotes_moc(<headers>)` and `pbnotes_rcc(<qrc>)` using `rcc --no-zstd`; ARM Qt root `${SYSROOT}/usr/local/qt6`, link `libQt6{Quick,Qml,Gui,Core,Concurrent}.so` by absolute path
- [ ] T005 [P] Vendor the SQLite amalgamation into `third_party/sqlite/sqlite3.c` and `third_party/sqlite/sqlite3.h` (download a pinned release from sqlite.org, do not copy InkFeed's file); record version, URL and SHA-256 in `third_party/sqlite/VERSION`
- [ ] T006 [P] Add the pinned Mozilla CA bundle `third_party/cacert/cacert.pem` (from curl.se, MPL-2.0); record download date, URL and SHA-256 in `third_party/cacert/VERSION`
- [ ] T007 [P] Create `NOTICE` with: InkFeed MIT copyright "Copyright (c) 2026 Joost Rohde" and the MIT permission text for adapted build/launcher/packaging patterns; Mozilla CA bundle (MPL-2.0); SQLite (public domain)
- [ ] T008 [P] Create `.gitignore` (`build-*/`, `pbnotes-data/`, `*.bak`, `dist/`)
- [ ] T009 [P] Create `tests/support/test_support.h` (minimal assertion macros, test registration, nonzero exit on failure) and `tests/CMakeLists.txt` registering each test executable with CTest

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core types, HTTP seam, CA selection, title rule, store skeleton, logging, device/server spikes

**CRITICAL**: No user story work starts until this phase is complete.

- [ ] T010 [P] Create `core/include/pbnotes/types.h`: `Note` {local_id, remote_id optional, status `Void|Edited|Deleted`, title, content, category, favorite, modified (epoch seconds), etag optional, excerpt}, `Account`, `SyncOutcome` {Never, Running, Success, Partial, Failed, Offline, Maintenance}, `SyncResult` {outcome, message_key, pushed, pulled, deleted, conflicts, recreated, error_class}, `ErrorClass` {None, Network, Tls, Auth, Maintenance, Unsupported, Server} (per `contracts/core-library.md`)
- [ ] T011 [P] Write failing unit test `tests/unit/test_title.cpp` for `derive_title`: first non-empty line, trimmed; leading blank lines skipped; whitespace-only text yields empty; multi-byte UTF-8 not cut mid-character
- [ ] T012 Implement `derive_title` in `core/include/pbnotes/title.h` and `core/src/title.cpp` (depends on T010, T011)
- [ ] T013 [P] Write failing unit test `tests/unit/test_ca_selector.cpp` for `select_ca_file`: uses the first device candidate that exists, is readable, is non-empty and contains a `-----BEGIN CERTIFICATE-----` marker; skips empty/unreadable/non-PEM candidates; falls back to the bundled path with source `Bundled`; never returns "no CA"
- [ ] T014 Implement `select_ca_file` in `core/include/pbnotes/ca_selector.h` and `core/src/ca_selector.cpp` (depends on T010, T013)
- [ ] T015 [P] Create `core/include/pbnotes/http_transport.h`: abstract `HttpTransport::perform(const Request&) -> Response` with `Request{method,url,headers,body,connect_timeout,total_timeout}`, `Response{status,headers,body,error_class}`; production implementations MUST refuse non-`https` URLs
- [ ] T016 [P] Create `tests/support/fake_transport.h`: scripted `FakeTransport` (queue of expected requests with canned responses, records every request incl. headers) used by contract and integration tests; accepts any URL scheme
- [ ] T017 [P] Write failing unit test `tests/unit/test_store_open.cpp`: `NoteStore::open` creates schema; verifies pragmas `journal_mode=TRUNCATE`, `synchronous=FULL`, `foreign_keys=ON`, `busy_timeout=5000`; sets `PRAGMA user_version`; copies the file to `pbnotes.db.bak` before a schema migration; integrity check failure is reported, not ignored
- [ ] T018 Implement `NoteStore::open` and schema in `core/include/pbnotes/note_store.h` and `core/src/note_store.cpp` linked to `third_party/sqlite/sqlite3.c`: table `account` (single row `id = 1`: server_url, username, password, collection_etag, collection_last_modified, api_versions, last_sync_at, last_sync_outcome in {never, success, partial, failed, offline, maintenance}, last_sync_message, last_sync_counts), table `notes` (local_id autoincrement PK, remote_id nullable unique, status in {void, edited, deleted}, title, content, category empty string = uncategorized, favorite 0/1, modified, etag nullable, excerpt) with indices on status, category, favorite, modified, remote_id, table `settings` (key/value) with defaults font_size=medium, sort_order=modified, view=all, rotation=auto; single SQLite connection guarded by one mutex (research R-4) (depends on T010, T017)
- [ ] T019 [P] Write failing unit test `tests/unit/test_log.cpp` and implement `core/include/pbnotes/log.h` + `core/src/log.cpp`: structured logger accepting only event codes, HTTP status, counts and file paths; any field named password, username or content is rejected/redacted (Principle III)
- [ ] T020 Write `scripts/probe-device.sh` (POSIX sh, run on the InkPad One): print Qt runtime version (`/usr/lib/libQt6Core.so*`), list `/usr/qml` and `/usr/plugins/platforms`, list candidate CA files (`/etc/ssl/certs/ca-certificates.crt`, `/etc/ssl/cert.pem`, anything under `/etc/ssl` and `/ebrmain` matching `*ca*`/`*cert*`), attempt an HTTPS handshake to a known site with each candidate if `curl` exists, and report free space on `/mnt/ext1`; no secrets printed
- [ ] T021 [MANUAL] Run `scripts/probe-device.sh` on the InkPad One and record results in `specs/001-pocketbook-notes-mvp/research.md` (R-2 Qt version, R-6 CA store, R-11 `QueryNetwork()` semantics); update the VERIFY items
- [ ] T022 [MANUAL] Against a Nextcloud test server with the Notes app, verify research R-7 items (a)-(e): `If-Match` on `PUT` returns 412 on mismatch, title derivation from the first line, whether `DELETE` honors `If-Match`, `pruneBefore` behavior, `chunkSize`/`chunkCursor` support; record anonymized responses as `tests/fixtures/*.json` and update `contracts/notes-api-v1.md` and `research.md`. If no server is available, hand-write the fixtures from the public API and mark them "unverified" in each fixture's file name suffix `.unverified.json`

**Checkpoint**: Foundation ready; user stories can begin. US1 → US2 → US3/US4 → US5 → US6 order is recommended (see Dependencies).

---

## Phase 3: User Story 1 - Connect to my notes server (Priority: P1) MVP

**Goal**: Enter server URL, username and app password, run a connection test with verified certificates, and save the account; ship a first installable app shell.

**Independent Test**: On host: contract and integration tests pass. On device: setup with valid details succeeds and shows the CA source; wrong password, unreachable host, `http://` URL and an untrusted certificate each show their own message and save nothing (quickstart step 5.2).

### Tests for User Story 1 (write first, must FAIL)

- [ ] T023 [P] [US1] Add fixtures `tests/fixtures/capabilities_ok.json` (notes `api_version` including `1.0`), `tests/fixtures/capabilities_no_notes.json`, `tests/fixtures/capabilities_only_v02.json`
- [ ] T024 [P] [US1] Contract test `tests/contract/test_api_capabilities.cpp` using `FakeTransport`: success; 401/403 → `Auth`; missing notes section or no `1.x` → `Unsupported`; 503 → `Maintenance`; transport error → `Network`; request carries Basic auth and `OCS-APIRequest: true`, `format=json`
- [ ] T025 [P] [US1] Unit test `tests/unit/test_connection_test.cpp` for URL validation/normalization: only `https://` accepted, `http://` rejected with an explanatory error key, trailing slash removed, sub-path kept, empty username/password rejected
- [ ] T026 [P] [US1] Integration test `tests/integration/test_tls_verification.cpp`: start a local HTTPS server with a self-signed certificate (generated with the `openssl` CLI; test is skipped with a message if `openssl` is absent) and assert `CurlTransport` fails with `Tls`, sends NO `Authorization` header (verified from the server's captured request bytes), and succeeds when the test CA is supplied via `CaSelection`
- [ ] T027 [P] [US1] Unit test `tests/unit/test_account_store.cpp`: save/get single account row (`id = 1`), second save replaces it, `forget_account` overwrites the stored password before deleting the row, password never appears in the logger output (T019)

### Implementation for User Story 1

- [ ] T028 [US1] Implement `CurlTransport` in `core/include/pbnotes/curl_transport.h` and `core/src/curl_transport.cpp`: `CURLOPT_SSL_VERIFYPEER=1`, `CURLOPT_SSL_VERIFYHOST=2`, `CURLOPT_CAINFO` from `CaSelection`, `CURLOPT_PROTOCOLS_STR="https"`, `CURLOPT_REDIR_PROTOCOLS_STR="https"`, `NOSIGNAL`, connect timeout 10 s, total timeout 60 s, `ACCEPT_ENCODING ""`, Basic auth via `CURLOPT_USERPWD`; no option or environment variable can disable verification; map curl errors to `ErrorClass` (depends on T014, T015, T026)
- [ ] T029 [US1] Implement `ApiClient::fetch_capabilities` in `core/include/pbnotes/api_client.h` and `core/src/api_client.cpp`, including a minimal defensive JSON parser in `core/src/json.cpp` (reject wrong types, no partial results) (depends on T015, T024)
- [ ] T030 [US1] Implement `ConnectionTester` in `core/include/pbnotes/connection_test.h` and `core/src/connection_test.cpp`: URL validation per T025, then capabilities; returns `{ok, error_class, message_key, ca_source}` (depends on T028, T029, T025)
- [ ] T031 [US1] Implement account persistence in `core/src/note_store.cpp`: `save_account`, `get_account`, `forget_account` (overwrite password then delete), enforcing exactly one account row (`id = 1`) (depends on T018, T027)
- [ ] T032 [US1] Create the Qt bridge base `bridge/include/notes_bridge.h` and `bridge/src/notes_bridge.cpp` with properties `configured`, `caSource`, `uiRotation` and invokables `testConnection`, `saveAccount`, `forgetAccount`, signal `connectionTested(ok, errorClass, message)`; connection test runs on a worker thread; never exposes the password back to QML (per `contracts/qml-bridge.md`)
- [ ] T033 [US1] Create `app/main.cpp`: `QGuiApplication`, data dir `/mnt/ext1/system/pbnotes` on device (`USERDATA "/pbnotes"`) or `./pbnotes-data` on host, `NoteStore::open`, bridge as context property `bridge`, load `qrc:/qml/Main.qml`; under `PLATFORM_POCKETBOOK`: G-sensor polling for `uiRotation` (500 ms), task icon, `quit()` = `ClearScreen(); FullUpdate(); CloseApp();`; call `QGuiApplication::styleHints()->setCursorFlashTime(0)` (pattern adapted from InkFeed `src/main_qml.cpp`, `src/qml_bridge.cpp`)
- [ ] T034 [P] [US1] Create `qml/InkButton.qml` (flat button: white fill, 2 px border, `#d8d8d8` when pressed, no animations; adapted from InkFeed) and `qml/Main.qml` skeleton: `ApplicationWindow` fullscreen white, stage `Item` scaled by `1/Screen.devicePixelRatio` and rotated by `bridge.uiRotation` (width/height swapped when sideways), `StackView` with all transitions removed, English strings through one `tr`-style function
- [ ] T035 [P] [US1] Create `qml/pages/SetupPage.qml`: fields server URL, username, app password (masked), "Test connection" and "Save" buttons, result area showing success + CA source or the classified error (rejected credentials / server not reachable / certificate could not be verified / unsupported server / `http://` not allowed), text stating the password is stored unencrypted on the device and should be an app password
- [ ] T036 [US1] Wire the build: `bridge/` and `app/` into `CMakeLists.txt` ARM branch (moc via `pbnotes_moc` for `notes_bridge.h`, qrc via `pbnotes_rcc` for `qml/qml.qrc` and `res/app.qrc`, output `pbnotes.app` ELF) and an optional host desktop app target when Qt6 dev packages exist; add `qml/qml.qrc`, `res/app.qrc` (depends on T003, T004, T032-T035)
- [ ] T037 [P] [US1] Create `platform/pocketbook/pbnotes.app` launcher (POSIX sh, adapted from InkFeed `platform/pocketbook/inkfeed.app`): create `/mnt/ext1/system/pbnotes`, redirect output to `pbnotes.log`, export `LD_LIBRARY_PATH="/usr/lib:/usr/local/lib:$LD_LIBRARY_PATH"`, `QT_QPA_PLATFORM_PLUGIN_PATH=/usr/plugins/platforms`, `QT_PLUGIN_PATH=/usr/plugins`, `QT_QPA_PLATFORM=pocketbook2egl`, `QML_IMPORT_PATH` and `QML2_IMPORT_PATH=/usr/qml`, `cd /mnt/ext1`, run `/mnt/ext1/applications/pbnotes.bin`, log exit code
- [ ] T038 [P] [US1] Create icons `res/icons/pbnotes.bmp` and `res/icons/pbnotes_f.bmp` (opaque BMP, 122x122, 8 bpp) via `scripts/make-icons.sh` (documented dependency; generated files committed)
- [ ] T039 [US1] Create `scripts/package-device.sh <elf> [version] [out-dir]` (adapted from InkFeed `scripts/package-inkpad-one.sh`): zip containing `applications/pbnotes.bin` (ELF, mode 777), `applications/pbnotes.app` (launcher, mode 777), `applications/icons/pbnotes.bmp` and `pbnotes_f.bmp`, `applications/pbnotes-cacert.pem` (from `third_party/cacert/cacert.pem`), `INSTALL.md`; output `<out-dir>/pbnotes-inkpad-one-<version>.zip` (depends on T037, T038)
- [ ] T040 [US1] Make the CA path available to the app: the bridge calls `select_ca_file` with the device candidates from research R-6 (updated by T021) and the bundled path `/mnt/ext1/applications/pbnotes-cacert.pem` (host: `third_party/cacert/cacert.pem`), and passes it to `CurlTransport` (depends on T014, T028, T032)
- [ ] T041 [US1] Write `INSTALL.md` (repo root, copied into the zip by T039): unzip onto `/mnt/ext1`, register in `system/config/desktop/view.json` with id `U_PbNotes`, absolute `path`, icon paths, add to a group such as `@General`, reboot; back up `view.json` first
- [ ] T042 [MANUAL] [US1] Build with `POCKETBOOK_SDK` set, install on the device, run quickstart step 5.2; record the outcome and any Qt/rcc incompatibility (research R-2, R-3)

**Checkpoint**: US1 works on host (tests) and on device (setup screen, connection test).

---

## Phase 4: User Story 2 - Read my notes, also offline (Priority: P1)

**Goal**: After setup, download all notes and read them in a large, e-ink-friendly reader, with the network off.

**Independent Test**: With sample notes on the server run the first sync, switch Wi-Fi off, open several notes; all display fully (quickstart 5.3). Host: pull integration tests and markdown tests pass.

### Tests for User Story 2 (write first, must FAIL)

- [ ] T043 [P] [US2] Add fixtures `tests/fixtures/notes_list_full.json`, `tests/fixtures/notes_list_pruned.json` (unchanged notes with only `id`), `tests/fixtures/note_single.json` (anonymized)
- [ ] T044 [P] [US2] Contract test `tests/contract/test_api_list.cpp`: `GET notes` with `pruneBefore` and `If-None-Match`; 200 parses notes and headers `ETag`, `Last-Modified`, `X-Notes-API-Versions`; 304 yields "unchanged"; 503 → `Maintenance`; invalid JSON → `Server`; `get_note` 200/404
- [ ] T045 [P] [US2] Unit test `tests/unit/test_markdown.cpp`: headings (levels), bold, italic, bullet and numbered lists (nesting depth), task items `[ ]`/`[x]` with source line index, links rendered as their label text, unsupported constructs (tables, code blocks, images) remain plain text, HTML-special characters preserved as text
- [ ] T046 [P] [US2] Unit test `tests/unit/test_store_pull.cpp`: `apply_remote` inserts unknown remote ids as `void`, updates a `void` note only if a field differs, leaves `edited`/`deleted` notes untouched, deletes `void` notes missing from the remote id list, keeps pending notes, stores collection ETag/Last-Modified and `api_versions`
- [ ] T047 [P] [US2] Integration test `tests/integration/test_sync_pull.cpp`: `SyncEngine::run` (pull path) against `FakeTransport` for first pull (no stored ETag), incremental pull with `pruneBefore`, 304, 503, offline; unsupported API version (no `1.x`) makes no note request and returns `Unsupported`; reading after the transport is made to fail still works from the store
- [ ] T048 [P] [US2] Benchmark test `tests/unit/test_perf_read.cpp`: with 1,000 generated notes of about 3 KB, `get` + markdown parse of one note completes well under the 3 s budget (SC-003)

### Implementation for User Story 2

- [ ] T049 [US2] Implement `ApiClient::list_notes` and `get_note` in `core/src/api_client.cpp` (per `contracts/notes-api-v1.md`; timestamps are epoch seconds) (depends on T044)
- [ ] T050 [US2] Implement `NoteStore::apply_remote`, `get`, `list` (unfiltered, sorted by modified desc), pull bookkeeping, and `excerpt` generation (first ~120 chars after the title) in `core/src/note_store.cpp` (depends on T046)
- [ ] T051 [US2] Implement the pull half of `SyncEngine` in `core/include/pbnotes/sync_engine.h` and `core/src/sync_engine.cpp`: capabilities/version gate (require `1.x`, else `Unsupported`), single-flight guard, list with stored ETag/`pruneBefore`, apply results, treat 304/503/offline as non-errors with `Offline`/`Maintenance`/`Success` outcomes and update the account's `last_sync_*` fields (depends on T049, T050, T047)
- [ ] T052 [P] [US2] Implement `parse_markdown` in `core/include/pbnotes/markdown.h` and `core/src/markdown.cpp` (blocks + spans per `contracts/core-library.md`; `toggle_task_item` is added in US6) (depends on T045)
- [ ] T053 [US2] Extend the bridge: `notes` list model (`bridge/include/note_list_model.h`, `bridge/src/note_list_model.cpp`: localId, title, category, modified, favorite, pending, excerpt), `openNote(localId)` returning rendered blocks via `bridge/include/markdown_model.h` (spans converted to HTML-escaped styled text), `fontSize`/`fontPx` with `setFontSize` persisting setting `font_size` in {`small`, `medium`, `large`} default `medium`, first sync after `saveAccount` on a worker thread (depends on T032, T050, T051, T052)
- [ ] T054 [P] [US2] Create `qml/pages/NoteListPage.qml`: large rows with title, category, modification date and a favorite marker; no highlight animation; touch targets about 8 mm; empty state text
- [ ] T055 [P] [US2] Create `qml/pages/ReaderPage.qml`: non-interactive `Flickable` with page-up/page-down buttons and tap zones that set `contentY` instantly, block rendering for headings/emphasis/lists/task boxes (read-only here), links as plain text, font-size control (small/medium/large) persisted through the bridge, no animations
- [ ] T056 [US2] Wire navigation in `qml/Main.qml`: setup page when `!bridge.configured`, else list → reader via `StackView` (no transitions); add `qml/pages/*.qml` to `qml/qml.qrc` (depends on T054, T055)
- [ ] T057 [MANUAL] [US2] Device check of quickstart 5.3 (read offline, markdown rendering, font sizes, restart); record rendering/ghosting observations in `research.md` R-10

**Checkpoint**: US1 + US2 deliver a read-only offline notes viewer (suggested MVP demo).

---

## Phase 5: User Story 3 - Write and edit notes offline (Priority: P2)

**Goal**: Create, edit, delete (with confirmation), set category and toggle favorite offline with explicit save; changes persist and are marked pending.

**Independent Test**: Wi-Fi off; create, edit, categorize, favorite and delete notes; restart; all changes persist and are marked as not yet synced (quickstart 5.4).

### Tests for User Story 3 (write first, must FAIL)

- [ ] T058 [P] [US3] Unit test `tests/unit/test_store_edit.cpp`: `create_local` rejects empty or whitespace-only content (FR-020); `save_local` sets status `edited`, new `modified`, recomputes title with `derive_title` and excerpt; `mark_deleted` removes the row if it has no `remote_id`, otherwise sets status `deleted`; toggling favorite and category are `edited` saves; pending notes survive store reopen
- [ ] T059 [P] [US3] Unit test `tests/unit/test_category_validation.cpp`: category maximum 255 characters, no leading or trailing space; invalid value keeps the previous category and returns an error key
- [ ] T060 [P] [US3] Unit test `tests/unit/test_store_failure.cpp`: when a write fails (simulated disk-full via a failing VFS or read-only DB), the previous version of the note is preserved and the error is reported (edge case: device out of storage)

### Implementation for User Story 3

- [ ] T061 [US3] Implement `create_local`, `save_local`, `mark_deleted` and category validation in `core/src/note_store.cpp` (atomic transactions; constraints quoted in T058/T059) (depends on T058, T059, T060)
- [ ] T062 [US3] Extend the bridge with `loadForEdit`, `saveNote(localId, text, category, favorite)` (`-1` creates; empty new notes rejected), `deleteNote`, `setCategory`, `toggleFavorite`, properties `pendingCount`, signals `noteChanged`, `errorOccurred` in `bridge/src/notes_bridge.cpp` (depends on T061)
- [ ] T063 [P] [US3] Create `qml/pages/EditorPage.qml`: plain `TextArea`, `Qt.inputMethod.show()` on open, explicit Save button, no timers, no live rendering, leave-with-unsaved-changes dialog (Save / Discard), Delete button with confirmation dialog, category prompt, favorite toggle
- [ ] T064 [US3] Add "new note" button and edit/delete entry points to `qml/pages/NoteListPage.qml` and `qml/pages/ReaderPage.qml`; show a "not yet synced" marker for pending notes in the list (FR-010); update `qml/qml.qrc` (depends on T063)
- [ ] T065 [MANUAL] [US3] Device check of quickstart 5.4 and the typing check 5.8; record keystroke latency and ghosting in `research.md` R-13 #2 and decide whether the `FullUpdate` fallback is needed

**Checkpoint**: Offline create/edit/delete works; changes wait for sync.

---

## Phase 6: User Story 4 - Keep notes in sync with the server (Priority: P2)

**Goal**: "Sync now" and sync at start with Wi-Fi: push local changes first, then pull; always show last sync time and status.

**Independent Test**: Make changes on device and server, tap Sync now, verify both sides match and the status line shows time and success (quickstart 5.5).

### Tests for User Story 4 (write first, must FAIL)

- [ ] T066 [P] [US4] Add fixtures `tests/fixtures/note_created.json`, `tests/fixtures/note_updated.json` (with new `etag`/`modified`/`title`)
- [ ] T067 [P] [US4] Contract test `tests/contract/test_api_write.cpp`: `POST notes` body has content, category, favorite, modified and NO title; `PUT notes/{id}` sends `If-Match` with the stored ETag unchanged; `DELETE notes/{id}` (404 treated as success); response fields replace local ones
- [ ] T068 [P] [US4] Integration test `tests/integration/test_sync_push.cpp`: push happens before pull (request order asserted); new note → POST and `remote_id` stored; edited note with remote id → PUT; deleted with remote id → DELETE then row removed; deleted without remote id → row removed without any request; a failing note does not stop the others and stays pending; outcome `Partial`
- [ ] T069 [P] [US4] Integration test `tests/integration/test_sync_robustness.cpp`: transport failure injected at every request index leaves no lost, duplicated or corrupted note (SC-005); a second `run` during a run returns "already running" without a second request; a note edited while its PUT is in flight stays `edited` (FR-036); 401 → `Failed/Auth` and local data untouched; TLS failure → `Failed/Tls`; offline → `Offline`; 503 → `Maintenance`; 304 → `Success`
- [ ] T070 [P] [US4] Unit test `tests/unit/test_sync_status.cpp`: status text/state for never synced, running, success, partial, failed, offline, maintenance, with counts `{pushed, pulled, deleted, conflicts}` persisted in `last_sync_counts`

### Implementation for User Story 4

- [ ] T071 [US4] Implement `ApiClient::create_note`, `update_note` (with `If-Match`), `delete_note` in `core/src/api_client.cpp` (depends on T067)
- [ ] T072 [US4] Implement the push half of `SyncEngine` in `core/src/sync_engine.cpp`: per-note transactions, `confirm_push` in `core/src/note_store.cpp` keeping status `edited` if content/category/favorite differ from the sent snapshot, 404-on-PUT re-create with POST, status/count bookkeeping; a 412 response is NOT overwritten and the note stays `edited` until US5 adds conflict handling (depends on T068, T069, T071)
- [ ] T073 [US4] Extend the bridge: `syncNow`, `syncRunning`, `syncStatus` (time + outcome, never empty), `syncOutcome`, `pendingCount`, `syncFinished` signal; run sync on a worker thread; auto-sync at start — on device only when `QueryNetwork()` reports `NET_CONNECTED`, never calling `NetConnect*`; host always attempts; no error when offline at start (FR-024) in `bridge/src/notes_bridge.cpp` and `app/main.cpp` (depends on T070, T072)
- [ ] T074 [P] [US4] Add a status bar to `qml/Main.qml` visible on every main-view screen (SC-010): last sync time and outcome, pending count, "Sync now" button disabled while running
- [ ] T075 [MANUAL] [US4] Device check of quickstart 5.5 and SC-009 timing with 1,000 notes; record results in `quickstart.md` notes

**Checkpoint**: Full read/write/sync loop works without conflict handling (data is never overwritten because `If-Match` is always sent).

---

## Phase 7: User Story 5 - Never lose a note to a conflict (Priority: P2)

**Goal**: When a note changed on both sides, keep both versions (server version stays, device version becomes an uploaded conflict copy) and tell the user.

**Independent Test**: Edit the same note on server and device (offline), sync: both texts exist, the copy is named as a conflict copy and also on the server, status reports the conflict (quickstart 5.6).

### Tests for User Story 5 (write first, must FAIL)

- [ ] T076 [P] [US5] Add fixtures `tests/fixtures/put_412.json`, `tests/fixtures/note_server_changed.json`, `tests/fixtures/delete_404.json`
- [ ] T077 [P] [US5] Integration test `tests/integration/test_sync_conflict.cpp`: PUT → 412 → exactly one `GET notes/{id}`, original replaced by the server version (status `void`), one new local note with the local text, category and favorite, status `edited` and no `remote_id`, pushed with POST in the same run, `conflicts` count 1; running again creates no further copy
- [ ] T078 [P] [US5] Unit test `tests/unit/test_conflict_title.cpp`: copy title is `<original title> (conflict YYYY-MM-DD)` written into the first line of the copy's content; if that title already exists locally append ` #2`, ` #3`, ...
- [ ] T079 [P] [US5] Integration test `tests/integration/test_sync_delete_edit.cpp`: PUT → 404 (deleted on server, edited locally) re-creates with POST and reports it; local delete of a note changed on the server: `GET` with `If-None-Match` returns 200 → server version restored as `void`, no DELETE sent, user informed; `GET` 304 → DELETE sent and row removed; `GET` 404 → row removed; if fixtures from T022 show `DELETE` honors `If-Match`, the test uses that path instead

### Implementation for User Story 5

- [ ] T080 [US5] Implement `create_conflict_copy` and the conflict title rule in `core/src/note_store.cpp` (depends on T078)
- [ ] T081 [US5] Implement 412 handling, 404 re-creation reporting and the delete re-check in `core/src/sync_engine.cpp` per `research.md` R-8 (depends on T072, T077, T079, T080)
- [ ] T082 [US5] Extend the bridge with `conflictCount` and signal `conflictsDetected(count)` in `bridge/src/notes_bridge.cpp` (depends on T081)
- [ ] T083 [P] [US5] Show conflict outcome in the status bar of `qml/Main.qml` ("N conflicts - copies kept") and a one-time dialog after a sync with conflicts or restored-deleted notes; conflict copies are ordinary notes found by title in `qml/pages/NoteListPage.qml`
- [ ] T084 [MANUAL] [US5] Device check of quickstart 5.6 including delete-versus-edit in both directions; confirm the copy appears in the Nextcloud web UI

**Checkpoint**: No silent overwrite is possible in any tested scenario (SC-006).

---

## Phase 8: User Story 6 - Organize and find notes (Priority: P3)

**Goal**: Views (All, Favorites, Uncategorized, category), sort by modified or title, substring search, and toggling task-list checkboxes in the reader.

**Independent Test**: With notes in several categories and favorites, switch views, change sort, search a word that is only in a body, toggle a checkbox and sync (quickstart 5.7).

### Tests for User Story 6 (write first, must FAIL)

- [ ] T085 [P] [US6] Unit test `tests/unit/test_store_views.cpp`: views `all`, `favorites`, `uncategorized`, `category:<name>` return the right notes (deleted notes excluded); sort `modified` newest first and `title` alphabetical; `categories()` lists non-empty categories with counts; setting values `view`, `sort_order` persist
- [ ] T086 [P] [US6] Unit test `tests/unit/test_store_search.cpp`: case-insensitive substring match on title and content including non-ASCII letters; matches only the body word; works with all other filters; 1,000-note search completes under the 2 s budget (SC-008)
- [ ] T087 [P] [US6] Unit test `tests/unit/test_markdown_toggle.cpp`: `toggle_task_item(text, line)` flips only `[ ]` ↔ `[x]` on that line, leaves every other byte unchanged, returns the text unchanged for a non-task line
- [ ] T088 [P] [US6] Integration test `tests/integration/test_toggle_sync.cpp`: toggling saves the note as `edited` and the next sync pushes it with `If-Match`

### Implementation for User Story 6

- [ ] T089 [US6] Implement view filters, sort, search and `categories()` in `core/src/note_store.cpp` (case folding for non-ASCII done in application code, not SQLite `LIKE`) (depends on T085, T086)
- [ ] T090 [P] [US6] Implement `toggle_task_item` in `core/src/markdown.cpp` (depends on T087)
- [ ] T091 [US6] Extend the bridge: `view`, `sortOrder`, `searchText`, `categories`, `setView`, `setSortOrder`, `setSearch`, `toggleTask` in `bridge/src/notes_bridge.cpp` (depends on T088, T089, T090)
- [ ] T092 [P] [US6] Add a view picker (All, Favorites, Uncategorized, each category with count), sort control and search field to `qml/pages/NoteListPage.qml`; make task boxes tappable in `qml/pages/ReaderPage.qml`
- [ ] T093 [MANUAL] [US6] Device check of quickstart 5.7 and SC-011 usability pass

**Checkpoint**: All six user stories are complete.

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Release-readiness, constitution compliance, documentation

- [ ] T094 [P] Write `README.md`: purpose, prerequisites (cmake, ninja or make, `qt6-base-dev-tools`, curl dev package), host build + tests, device build (`POCKETBOOK_SDK` / `-DSDK_PATH`), packaging, install and `view.json` registration, a prominent note that the password is stored UNENCRYPTED in `pbnotes.db` on the device's FAT partition, recommended use of an app password and how to "Forget account", a "License: MIT" section pointing to `LICENSE`
- [ ] T095 [P] Create `scripts/check-constitution.sh` and run it in CTest: fails if sources contain `VERIFYPEER`/`VERIFYHOST` set to 0 or `CURLOPT_SSL_VERIFYSTATUS`-style bypasses, `pocketbook-sdk` or `/opt/pocketbook` literals in build files, Qt/inkview includes under `core/`, or note-content/password logging calls
- [ ] T096 [P] Edge-case tests in `tests/integration/test_edge_cases.cpp`: very long note (tens of thousands of characters) saves and parses, duplicate titles stay distinct, wrong device clock does not cause loss, category with `/`, empty category, and 255+ character category per the validation rules
- [ ] T097 [P] Host benchmark `tests/unit/test_perf_search_sync.cpp`: 1,000-note search < 2 s (SC-008) and a simulated 1,000-note sync with a few changes completes quickly against `FakeTransport` (SC-009 host portion)
- [x] T098 Document the MIT license (decided 2026-10-10): `LICENSE` exists with "Copyright (c) 2026 Volker von Einem" and `README.md` states "MIT License" and links `LICENSE`. The `NOTICE` file itself is created by T007 and MUST list only third-party items; re-check that no file claims a different license before the first public push
- [ ] T099 Security review: confirm no secrets or real user data in the repository or fixtures, logs contain no personal data (T019), TLS never bypassed (T095), password forgotten correctly (T027); record findings in `docs/security-review.md`
- [ ] T100 [MANUAL] Run the full `quickstart.md` on the device (steps 2-8) and SC-001, SC-003, SC-007, SC-009; update `research.md` R-13 with observed risks
- [ ] T101 Update `docs/inkfeed-analysis.md` / `docs/pocketbook-sdk-analysis.md` references and `research.md` VERIFY items with confirmed facts after T021, T022, T042

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: none; start immediately.
- **Foundational (Phase 2)**: after Setup; blocks all stories. T021 and T022 are manual spikes: stories may start in parallel with them, but the TLS/CA and API-fixture tasks (T040, T022-dependent fixtures) must be revisited when results arrive.
- **US1 (P1)**: after Foundational. First installable device build.
- **US2 (P1)**: after Foundational; needs US1 only for account/setup UI and the shared bridge/app shell (the core pull path can be built and tested without it).
- **US3 (P2)**: after US2's store/list/reader (it edits notes shown there); core part independent.
- **US4 (P2)**: after US2 (sync engine pull) and US3 (pending changes to push).
- **US5 (P2)**: after US4 (push path) .
- **US6 (P3)**: after US2 (list/reader); independent of US3-US5 in the core; toggle uses the edit path from US3.
- **Polish**: after the desired stories; T098 gates the first public push.

### Within Each User Story

- Test tasks first and observed failing, then store/API → engine → bridge → QML → device check.
- Same-file tasks are sequential (for example `core/src/note_store.cpp` is touched by T018, T031, T050, T061, T080, T089; `core/src/sync_engine.cpp` by T051, T072, T081; `bridge/src/notes_bridge.cpp` by T032, T053, T062, T073, T082, T091).

### Parallel Opportunities

- Setup: T003-T009 in parallel after T001/T002.
- Foundational: T010, T011, T013, T015, T016, T017, T019 in parallel; T020 anytime.
- Per story: all fixture/test tasks marked [P] together; QML pages marked [P] together once the bridge contract exists.
- After US2: US3 and US6 core work (store tests T058-T060, T085-T087) can proceed in parallel by different people.

### Parallel Example: User Story 2

```bash
# Tests together (different files):
Task: "Contract test tests/contract/test_api_list.cpp"            # T044
Task: "Unit test tests/unit/test_markdown.cpp"                   # T045
Task: "Unit test tests/unit/test_store_pull.cpp"                 # T046
Task: "Integration test tests/integration/test_sync_pull.cpp"    # T047
# QML pages together after the bridge compiles:
Task: "qml/pages/NoteListPage.qml"                               # T054
Task: "qml/pages/ReaderPage.qml"                                 # T055
```

---

## Implementation Strategy

### MVP First (User Stories 1 and 2)

1. Phase 1 Setup, Phase 2 Foundational (host tests green, spikes recorded).
2. US1 → device build proves the toolchain, Qt runtime and TLS/CA decision (T042).
3. US2 → read-only offline viewer on the device. STOP and validate (T057).
4. Then US3 + US4 for the usable read/write/sync product, US5 for conflict safety. Do not release or sync real data before US5 (US4 never overwrites, but conflicting edits are only kept as unsent edits until US5).

### Incremental Delivery

Each phase ends at a checkpoint that is demonstrable on host (CTest) and on device (quickstart step). Commit after each task or logical group.

### Notes

- [P] tasks touch different files. Verify each test fails before implementing (Principle I).
- Task coverage of spec requirements: FR-001..005 (US1), FR-006, 011-014 (US2), FR-007-009 (US6), FR-010, 015-022 (US3), FR-023-031 (US4), FR-032-036 (US5), FR-037 and FR-031 are satisfied by not building the excluded features.
- Avoid: vague tasks, same-file conflicts, cross-story dependencies that break independence.
