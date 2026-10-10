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
relative to the repository root.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US6 as in spec.md
- **[MANUAL]**: needs a person, the device, or a live Nextcloud server; record results in the named
  file. An implementing agent skips these and reports them as open

## Conventions that apply to every task

- C++17; namespace `pbnotes`; `notes_core` MUST NOT include Qt or `inkview.h` headers.
- TLS verification MUST never be disabled. Forbidden forms, scanned by T095 in code AND shell scripts: `curl -k`, `curl --insecure`, `CURLOPT_SSL_VERIFYPEER` / `CURLOPT_SSL_VERIFYHOST` set to 0, and `CURLOPT_SSL_VERIFYSTATUS`-style bypasses (`CURLOPT_SSL_VERIFYSTATUS`, `CURLOPT_PROXY_SSL_VERIFYPEER`, `CURLOPT_PROXY_SSL_VERIFYHOST`, `CURLOPT_DOH_SSL_VERIFYPEER`, `CURLOPT_DOH_SSL_VERIFYHOST`, `CURLOPT_DOH_SSL_VERIFYSTATUS` set to 0/false).
- SDK paths MUST NOT be hard-coded; only `-DSDK_PATH` / `POCKETBOOK_SDK`.
- Logs MUST NOT contain note text, passwords, or user names.
- Do not copy code or text from notes-android; InkFeed patterns are adapted and attributed in `NOTICE`.

---

## Phase 1: Setup & Hello World (Shared Infrastructure)

**Purpose**: Repository skeleton, build wiring, vendored third-party files, a Hello World app that is built for ARM, packaged and started on the InkPad One before any further work (de-risks the SDK, host `rcc`, device Qt runtime, launcher and packaging), and a typing spike that proves a multi-line `TextArea` with the system on-screen keyboard works under the rotated, scaled stage

- [x] T001 Create the directory skeleton from plan.md: `core/include/pbnotes/`, `core/src/`, `third_party/sqlite/`, `third_party/cacert/`, `bridge/include/`, `bridge/src/`, `app/`, `qml/`, `res/icons/`, `platform/pocketbook/`, `scripts/`, `cmake/`, `tests/unit/`, `tests/contract/`, `tests/integration/`, `tests/fixtures/`, `tests/support/` (add `.gitkeep` where empty)
- [x] T002 Create top-level `CMakeLists.txt` (min 3.16, C++17, `-DTARGET_TYPE=ARM` vs host, `enable_testing()`, variable `PBNOTES_APP_NAME=pbnotes`, target `notes_core` (static lib), test executables registered with CTest (T009); ARM branch includes `cmake/sdk.cmake` before `project()`; host build MUST NOT require Qt or the SDK)
- [x] T003 [P] Create `cmake/sdk.cmake`: `SDK_PATH` from `-DSDK_PATH`, else environment variable `POCKETBOOK_SDK`; if neither is set stop with `message(FATAL_ERROR ...)` naming both options; NO default path. Derive `SYSROOT=${SDK_PATH}/arm-buildroot-linux-gnueabihf/sysroot`, compilers `${SDK_PATH}/bin/arm-buildroot-linux-gnueabihf-{gcc,g++}`, `CMAKE_SYSTEM_NAME Linux`, `CMAKE_SYSTEM_PROCESSOR arm`, `CMAKE_SYSROOT`, `CMAKE_FIND_ROOT_PATH`, find modes PROGRAM=NEVER / LIBRARY=ONLY / INCLUDE=ONLY (pattern adapted from InkFeed `CMakeLists.txt:9-25`)
- [x] T004 [P] Create `cmake/qt_manual.cmake`: `HOST_MOC` from `${SDK_PATH}/local/qt6/libexec` (NO_DEFAULT_PATH, REQUIRED); `HOST_RCC` searched in `/usr/lib/qt6/libexec`, `/usr/lib/x86_64-linux-gnu/qt6/libexec`, `/usr/lib/qt6/bin` (REQUIRED, error message names package `qt6-base-dev-tools`); print the found rcc version at configure time; functions `pbnotes_moc(<headers>)` and `pbnotes_rcc(<qrc>)` using `rcc --no-zstd`; ARM Qt root `${SYSROOT}/usr/local/qt6`, link `libQt6{Quick,Qml,Gui,Core,Concurrent}.so` by absolute path
- [x] T005 [P] Vendor the SQLite amalgamation into `third_party/sqlite/sqlite3.c` and `third_party/sqlite/sqlite3.h` (download a pinned release from sqlite.org, do not copy InkFeed's file); record version, URL and SHA-256 in `third_party/sqlite/VERSION`
- [x] T006 [P] Add the pinned Mozilla CA bundle `third_party/cacert/cacert.pem` (from curl.se, MPL-2.0); record download date, URL and SHA-256 in `third_party/cacert/VERSION`
- [x] T007 [P] Create `NOTICE` with: InkFeed MIT copyright "Copyright (c) 2026 Joost Rohde" and the MIT permission text for adapted build/launcher/packaging patterns; Mozilla CA bundle (MPL-2.0); SQLite (public domain)
- [x] T008 [P] Create `.gitignore` (`build-*/`, `pbnotes-data/`, `*.bak`, `dist/`)
- [x] T009 [P] Create `tests/support/test_support.h` (minimal assertion macros, test registration, nonzero exit on failure) and `tests/CMakeLists.txt` registering each test executable with CTest
- [x] T110 [P] Create the Hello World app: `app/main.cpp`, `app/hello_bridge.h`, `app/hello_bridge.cpp`. `QGuiApplication` + `QQmlApplicationEngine` loading `qrc:/qml/Main.qml`; a `HelloBridge` QObject (with `Q_OBJECT`, so moc is exercised) exposes `appVersion` (compile definition `PBNOTES_VERSION`, default `0.0.0-hello`), `qtRuntimeVersion` (`qVersion()`), `dataDir`, and invokable `quit()`; data dir `/mnt/ext1/system/pbnotes` on device (`USERDATA "/pbnotes"` under `PLATFORM_POCKETBOOK`) or `./pbnotes-data` on host; writes one startup line (versions, screen size, devicePixelRatio) to `<data dir>/hello.log`; `quit()` calls `ClearScreen(); FullUpdate(); CloseApp();` on device before `QCoreApplication::quit()`. No networking, SQLite or core library (pattern adapted from InkFeed `src/main_qml.cpp`)
- [x] T111 [P] Create the Hello World UI `qml/Main.qml` and `qml/qml.qrc`: fullscreen white `ApplicationWindow` with the text "pbnotes Hello World", lines for app version, Qt runtime version, screen size and devicePixelRatio, and a flat Quit button (`QtQuick.Controls` `Button`); imports only `QtQuick`, `QtQuick.Controls`, `QtQuick.Window`
- [x] T115 [P] Extend the Hello World bridge `app/hello_bridge.h` / `app/hello_bridge.cpp` (T110) for the typing spike: property `uiRotation` (0/90/180/270; under `PLATFORM_POCKETBOOK` poll `SetGSensorEnabled(true)` + `GetGSensorOrientation()` every 500 ms as InkFeed does, 0 on host), invokable `log(text)` appending a line to `<data dir>/hello.log`, and a call to `QGuiApplication::styleHints()->setCursorFlashTime(0)` at startup (depends on T110)
- [x] T117 [P] Create `qml/pages/EditorSpike.qml` and add it to `qml/qml.qrc`: a multi-line, wrapping plain `TextArea` that takes focus and calls `Qt.inputMethod.show()` when opened; live status line showing `Qt.inputMethod.visible`, `Qt.inputMethod.keyboardRectangle` (x, y, width, height), the TextArea geometry, character count and `uiRotation`; the caret/last line is kept visible while the keyboard is shown (resize or scroll the TextArea); `hello.log()` records, for each text change, the time until the next `QQuickWindow.frameSwapped` (app-side frame time, not panel latency); buttons: Hide keyboard / Show keyboard, Clear, Back (depends on T115)
- [x] T116 Extend `qml/Main.qml` (T111) with the stage pattern from InkFeed (`Main.qml:157-172`): a stage `Item` sized to the window times `Screen.devicePixelRatio`, `scale: 1/Screen.devicePixelRatio`, `rotation: hello.uiRotation`, width/height swapped when sideways; a `StackView` with all transitions removed; a "Typing spike" button that pushes `qml/pages/EditorSpike.qml`; the spike MUST run inside this stage so keyboard placement and touch hits are tested under rotation and scaling (depends on T115, T117)
- [x] T112 Wire the Hello World build in `CMakeLists.txt` (ARM branch, `TARGET_TYPE=ARM`): sources `app/*.cpp`, moc for `app/hello_bridge.h` via `pbnotes_moc` (T004), rcc for `qml/qml.qrc` via `pbnotes_rcc` (host `rcc` with `--no-zstd`), Qt libs linked by absolute path, `PLATFORM_POCKETBOOK` definition, output `pbnotes.app` ELF; `file build-arm/pbnotes.app` MUST report `ELF 32-bit LSB executable, ARM, EABI5`; add an optional host desktop target when Qt6 dev packages exist; the Qt-free host build of `notes_core` MUST keep working without Qt or the SDK (depends on T003, T004, T110, T111, T115, T116, T117)
- [x] T037 [P] Create `platform/pocketbook/pbnotes.app` launcher (POSIX sh, adapted from InkFeed `platform/pocketbook/inkfeed.app`): create `/mnt/ext1/system/pbnotes`, redirect output to `pbnotes.log`, export `LD_LIBRARY_PATH="/usr/lib:/usr/local/lib:$LD_LIBRARY_PATH"`, `QT_QPA_PLATFORM_PLUGIN_PATH=/usr/plugins/platforms`, `QT_PLUGIN_PATH=/usr/plugins`, `QT_QPA_PLATFORM=pocketbook2egl`, `QML_IMPORT_PATH` and `QML2_IMPORT_PATH=/usr/qml`, `cd /mnt/ext1`, run `/mnt/ext1/applications/pbnotes.bin`, log exit code
- [x] T038 [P] Create icons `res/icons/pbnotes.bmp` and `res/icons/pbnotes_f.bmp` (opaque BMP, 122x122, 8 bpp) via `scripts/make-icons.sh` (documented dependency; generated files committed)
- [x] T039 Create `scripts/package-device.sh <elf> [version] [out-dir]` (adapted from InkFeed `scripts/package-inkpad-one.sh`): zip containing `applications/pbnotes.bin` (ELF, mode 777), `applications/pbnotes.app` (launcher, mode 777), `applications/icons/pbnotes.bmp` and `pbnotes_f.bmp`, `applications/pbnotes-cacert.pem` (from `third_party/cacert/cacert.pem`), `INSTALL.md`; output `<out-dir>/pbnotes-inkpad-one-<version>.zip` (depends on T037, T038)
- [x] T113 Write `INSTALL.md` (repo root, copied into the zip by T039): unzip onto `/mnt/ext1`, register in `system/config/desktop/view.json` with id `U_PbNotes`, absolute `path`, icon paths, add to a group such as `@General`, reboot; back up `view.json` first (the custom-CA section is added later by T041)
- [x] T114 [MANUAL] Hello World gate: set `POCKETBOOK_SDK`, configure with `-DTARGET_TYPE=ARM`, build, package with `scripts/package-device.sh`, install on the InkPad One following `INSTALL.md`, start from Applications. Verify: the app starts without crashing, shows the text and the Qt runtime version, Quit returns cleanly to the launcher (no black panel), `pbnotes.log` shows exit code 0. In the same device session run T021 (probe) and T118 (typing spike). Record the host `rcc` version, the device Qt runtime version and any `rcc`/ABI/QML-import failure in `research.md` R-2 and R-3, and fix the build before continuing
- [x] T118 [MANUAL] Typing spike gate on the InkPad One (portrait AND landscape): open "Typing spike" and verify with recorded results (pass/fail per line in `research.md` R-10 and R-13 #2): (1) tapping the TextArea shows the system on-screen keyboard within 3 s (or `Qt.inputMethod.show()` does); (2) about 500 characters typed continuously arrive without lost or duplicated characters, Enter inserts a new line, backspace works including key repeat; (3) the caret line and the last typed text are not covered by the keyboard; (4) keyboard keys and the TextArea receive touches at the correct positions in both orientations (stage rotation and `1/devicePixelRatio` scaling are correct); (5) the caret does not blink; (6) ghosting and typing latency are acceptable or a workaround is chosen (delayed repaint, a `FullUpdate` action, or editing in a dialog) and written down; (7) umlauts and other non-ASCII input work, and long-press selection and paste behave sensibly; (8) hiding and re-showing the keyboard and returning to the Hello World page works repeatedly. If (1)-(4) fail, STOP: the editor design (US3) must be re-planned before further UI work. RESULT 2026-10-10: (1)-(4) pass once rotation uses SetOrientation + updateGeometry (research R-15); (6) latency about 45 ms app-side, ghosting over a long session still to be judged; (5), (7) and the rest of (8) were not checked

**Checkpoint**: The Hello World runs on the InkPad One (T114) and the typing spike passes (T118). Host-only work in Phase 2 may start in parallel, but US1 and every other device-facing task MUST wait for this gate, because it validates the SDK wiring, the host `rcc` output against the device Qt, the launcher and the packaging.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core types, HTTP seam, CA selection, title rule, store skeleton, logging, device/server spikes

**CRITICAL**: No user story work starts until this phase is complete.

- [x] T010 [P] Create `core/include/pbnotes/types.h`: `Note` {local_id, remote_id optional, status `Void|Edited|Deleted`, title, content, category, favorite, modified (epoch seconds), etag optional, excerpt}, `Account`, `SyncOutcome` {Never, Running, Success, Partial, Failed, Offline, Maintenance}, `SyncResult` {outcome, message_key, pushed, pulled, deleted, conflicts, recreated, error_class}, `ErrorClass` {None, Network, Tls, Auth, Maintenance, Unsupported, Server} (per `contracts/core-library.md`)
- [x] T011 [P] Write failing unit test `tests/unit/test_title.cpp` for `derive_title`: first non-empty line, trimmed; leading blank lines skipped; whitespace-only text yields empty; multi-byte UTF-8 not cut mid-character
- [x] T012 Implement `derive_title` in `core/include/pbnotes/title.h` and `core/src/title.cpp` (depends on T010, T011)
- [x] T013 [P] Write failing unit test `tests/unit/test_ca_selector.cpp` for `select_ca_file`: uses the first device candidate that exists, is readable, is non-empty and contains a `-----BEGIN CERTIFICATE-----` marker; skips empty/unreadable/non-PEM candidates; falls back to the bundled path with source `Bundled`; never returns "no CA"
- [x] T014 Implement `select_ca_file` in `core/include/pbnotes/ca_selector.h` and `core/src/ca_selector.cpp` (depends on T010, T013)
- [x] T015 [P] Create `core/include/pbnotes/http_transport.h`: abstract `HttpTransport::perform(const Request&) -> Response` with `Request{method,url,headers,body,connect_timeout,total_timeout}`, `Response{status,headers,body,error_class}`; production implementations MUST refuse non-`https` URLs
- [x] T016 [P] Create `tests/support/fake_transport.h`: scripted `FakeTransport` (queue of expected requests with canned responses, records every request incl. headers) used by contract and integration tests; accepts any URL scheme
- [x] T017 [P] Write failing unit test `tests/unit/test_store_open.cpp`: `NoteStore::open` creates schema; verifies pragmas `journal_mode=TRUNCATE`, `synchronous=FULL`, `foreign_keys=ON`, `secure_delete=ON`, `busy_timeout=5000`; sets `PRAGMA user_version`; copies the file to `pbnotes.db.bak` before a schema migration and deletes that backup once the migration and the integrity check have succeeded; integrity check failure is reported, not ignored
- [x] T018 Implement `NoteStore::open` and schema in `core/include/pbnotes/note_store.h` and `core/src/note_store.cpp` linked to `third_party/sqlite/sqlite3.c`: table `account` (single row `id = 1`: server_url, username, password, collection_etag, collection_last_modified, api_versions, last_sync_at, last_sync_outcome in {never, success, partial, failed, offline, maintenance}, last_sync_message, last_sync_counts), table `notes` (local_id autoincrement PK, remote_id nullable unique, status in {void, edited, deleted}, title, content, category empty string = uncategorized, favorite 0/1, modified, etag nullable, excerpt) with indices on status, category, favorite, modified, remote_id, table `settings` (key/value) with defaults font_size=medium, sort_order=modified, view=all, rotation=auto; single SQLite connection guarded by one mutex (research R-4) (depends on T010, T017)
- [x] T019 [P] Write failing unit test `tests/unit/test_log.cpp` and implement `core/include/pbnotes/log.h` + `core/src/log.cpp`: structured logger accepting only event codes, HTTP status, counts and file paths; any field named password, username or content is rejected/redacted (Principle III)
- [x] T020 Write `scripts/probe-device.sh` (POSIX sh, run on the InkPad One): print Qt runtime version (`/usr/lib/libQt6Core.so*`), list `/usr/qml` and `/usr/plugins/platforms`, list candidate CA files (`/etc/ssl/certs/ca-certificates.crt`, `/etc/ssl/cert.pem`, anything under `/etc/ssl` and `/ebrmain` matching `*ca*`/`*cert*`), attempt an HTTPS handshake to a known site with each candidate if `curl` exists, report free space on `/mnt/ext1`, and report whether `/mnt/ext1/system` exists and is writable (location for `custom-ca.pem`); no secrets printed; the script MUST NOT use `curl -k`, `--insecure` or any other verification bypass (enforced by the T095 scan, which includes `scripts/probe-device.sh`)
- [x] T021 [MANUAL] Run `scripts/probe-device.sh` on the InkPad One (PARTLY DONE through the Hello World logs: Qt version and `QueryNetwork()` semantics are recorded in research R-2/R-11/R-15; the CA store (R-6) is still open and is best checked by a real TLS request from the connection test) and record results in `specs/001-pocketbook-notes-mvp/research.md` (R-2 Qt version, R-6 CA store, R-11 `QueryNetwork()` semantics); update the VERIFY items
- [x] T022 [MANUAL] Against a Nextcloud test server with the Notes app, verify research R-7 items (a)-(e): `If-Match` on `PUT` returns 412 on mismatch, title derivation from the first line, whether `DELETE` honors `If-Match` (if a stale `If-Match` on `DELETE` returns 412, the client sends the header and drops the conditional-GET fallback), `pruneBefore` behavior, `chunkSize`/`chunkCursor` support; record anonymized responses as `tests/fixtures/*.json` and update `contracts/notes-api-v1.md` and `research.md`. If no server is available, hand-write the fixtures from the public API and mark them "unverified" in each fixture's file name suffix `.unverified.json`; run every fixture through the anonymization procedure and check of T123 before committing it **SKIPPED 2026-10-10 (decision of the maintainer): not run against a live server; the fixtures stay `*.unverified.json` and the VERIFY items (a)-(e) in research R-7 stay open. The conflict and delete scenarios were checked end to end on the device (R-18, R-19).**
- [x] T123 Create `tests/fixtures/README.md` (anonymization procedure: host names become `cloud.example.com`, user names `alice`, e-mail addresses `@example.com`, note text generated placeholder text, ETags regenerated, no `Authorization` values, no real server ids beyond small integers) and `scripts/check-fixtures.sh` registered in CTest, which fails if a fixture contains a host other than `example.com`, `example.org`, `localhost` or `*.invalid`, an e-mail address outside `@example.com`, or an `Authorization` value (constitution Principle IV: anonymized fixtures only)

**Checkpoint**: Foundation ready; user stories can begin. US1 → US2 → US3/US4 → US5 → US6 order is recommended (see Dependencies).

---

## Phase 3: User Story 1 - Connect to my notes server (Priority: P1) MVP

**Goal**: Enter server URL, username and app password, run a connection test with verified certificates, and save the account; ship a first installable app shell.

**Independent Test**: On host: contract and integration tests pass. On device: setup with valid details succeeds and shows the CA source; wrong password, unreachable host, `http://` URL and an untrusted certificate each show their own message and save nothing (quickstart step 5.2).

### Tests for User Story 1 (write first, must FAIL)

- [x] T023 [P] [US1] Add fixtures `tests/fixtures/capabilities_ok.json` (notes `api_version` including `1.0`), `tests/fixtures/capabilities_no_notes.json`, `tests/fixtures/capabilities_only_v02.json`
- [x] T024 [P] [US1] Contract test `tests/contract/test_api_capabilities.cpp` using `FakeTransport`: success; 401/403 → `Auth`; missing notes section or no `1.x` → `Unsupported`; 503 → `Maintenance`; transport error → `Network`; request carries Basic auth and `OCS-APIRequest: true`, `format=json`
- [x] T025 [P] [US1] Unit test `tests/unit/test_connection_test.cpp` for URL validation/normalization: only `https://` accepted, `http://` rejected with an explanatory error key, trailing slash removed, sub-path kept, empty username/password rejected
- [x] T026 [P] [US1] Integration test `tests/integration/test_tls_verification.cpp`: start a local HTTPS server with a self-signed certificate (generated with the `openssl` CLI; test is skipped with a message if `openssl` is absent) and assert `CurlTransport` fails with `Tls`, sends NO `Authorization` header (verified from the server's captured request bytes), and succeeds when the test CA is supplied via `CaSelection`
- [x] T027 [P] [US1] Unit test `tests/unit/test_account_store.cpp`: save/get single account row (`id = 1`), second save replaces it, `forget_account` overwrites the stored password before deleting the row, password never appears in the logger output (T019); after `forget_account` (and after a password change) the old password string is found NOWHERE in the raw bytes of the database file, its journal file or `pbnotes.db.bak`
- [x] T103 [P] [US1] Integration test `tests/integration/test_tls_cert_failures.cpp`: with local HTTPS servers (certificates generated with the `openssl` CLI; skipped with a message if `openssl` is absent) assert that an EXPIRED certificate and a certificate for the WRONG HOSTNAME (both signed by a CA the client trusts) are refused with `Tls`, and that no `Authorization` header reaches either server (verified from captured request bytes)
- [x] T104 [P] [US1] Integration test `tests/integration/test_redirect_credentials.cpp`: server A (trusted) answers with a redirect to server B on a different host/port; assert the request to B carries NO `Authorization` header (credentials are never forwarded to another host), and that a redirect to an `http://` URL is refused
- [x] T105 [P] [US1] Unit test `tests/unit/test_effective_ca.cpp` for `build_effective_ca`: effective bundle = base CA file (device store if usable, else bundled) followed by the contents of `custom-ca.pem`; no `custom-ca.pem` → effective bundle equals the base; an empty, unreadable, larger-than-1-MB or non-PEM-certificate custom file is ignored with status `Invalid` and the base is still used (verification never off); the effective file is written atomically and regenerated when either input changes; the input files are never modified; the result reports the number of certificates found in the custom file (counted from `-----BEGIN CERTIFICATE-----` markers)
- [x] T106 [P] [US1] Integration test `tests/integration/test_private_ca.cpp` with a locally generated private CA (`openssl` CLI, skipped if absent): a server certificate signed by that CA is refused (`Tls`) without `custom-ca.pem`, accepted with it, still refused with a different CA's `custom-ca.pem`, a wrong-hostname certificate from the same private CA is still refused even with `custom-ca.pem` loaded, and a self-signed server certificate (with a matching subject alternative name) placed in `custom-ca.pem` is accepted while the same certificate without the file is refused

### Implementation for User Story 1

- [x] T028 [US1] Implement `CurlTransport` in `core/include/pbnotes/curl_transport.h` and `core/src/curl_transport.cpp`: `CURLOPT_SSL_VERIFYPEER=1`, `CURLOPT_SSL_VERIFYHOST=2`, `CURLOPT_CAINFO` from `CaSelection`, `CURLOPT_PROTOCOLS_STR="https"`, `CURLOPT_REDIR_PROTOCOLS_STR="https"`, `NOSIGNAL`, connect timeout 10 s, total timeout 60 s, `ACCEPT_ENCODING ""`, Basic auth via `CURLOPT_USERPWD`; no option or environment variable can disable verification; map curl errors to `ErrorClass` (depends on T014, T015, T026)
- [x] T029 [US1] Implement `ApiClient::fetch_capabilities` in `core/include/pbnotes/api_client.h` and `core/src/api_client.cpp`, including a minimal defensive JSON parser in `core/src/json.cpp` (reject wrong types, no partial results) (depends on T015, T024)
- [x] T030 [US1] Implement `ConnectionTester` in `core/include/pbnotes/connection_test.h` and `core/src/connection_test.cpp`: URL validation per T025, then capabilities; returns `{ok, error_class, message_key, ca_source}` (depends on T028, T029, T025)
- [x] T031 [US1] Implement account persistence in `core/src/note_store.cpp`: `save_account`, `get_account`, `forget_account` (overwrite the password, then delete the row; `secure_delete=ON` zeroes freed pages), enforcing exactly one account row (`id = 1`) (depends on T018, T027)
- [x] T107 [US1] Implement `build_effective_ca(base_ca_path, custom_ca_path, out_path)` in `core/include/pbnotes/ca_selector.h` and `core/src/ca_selector.cpp`: concatenates the base CA file and a valid `custom-ca.pem` into `out_path` (`<data dir>/effective-ca.pem`, atomic write), returns `{path, custom_status: None|Loaded|Invalid, custom_cert_count}`; the base alone is used when the custom file is absent or invalid; there is no code path that disables verification (depends on T014, T105)
- [x] T032 [US1] Create the Qt bridge base `bridge/include/notes_bridge.h` and `bridge/src/notes_bridge.cpp` with properties `configured`, `caSource`, `orientation` (informational) and invokables `testConnection`, `saveAccount`, `forgetAccount`, signal `connectionTested(ok, errorClass, message)`; connection test runs on a worker thread; never exposes the password back to QML (per `contracts/qml-bridge.md`)
- [x] T033 [US1] Extend `app/main.cpp` (created as Hello World by T110): `QGuiApplication`, data dir `/mnt/ext1/system/pbnotes` on device (`USERDATA "/pbnotes"`) or `./pbnotes-data` on host, `NoteStore::open`, bridge as context property `bridge`, load `qrc:/qml/Main.qml`; under `PLATFORM_POCKETBOOK`: keep the orientation handling of the Hello World bridge (T115 and the rotation work of research R-15): G-sensor poll every 500 ms, `SetOrientation(n)`, then the platform screen's `updateGeometry()` via `QMetaObject`, the keyboard closed around the change and reopened, NO content rotation, task icon, `quit()` = `ClearScreen(); FullUpdate(); CloseApp();`; call `QGuiApplication::styleHints()->setCursorFlashTime(0)` (pattern adapted from InkFeed `src/main_qml.cpp`, `src/qml_bridge.cpp`)
- [x] T034 [P] [US1] Create `qml/InkButton.qml` (flat button: white fill, 2 px border, `#d8d8d8` when pressed, no animations; adapted from InkFeed) and extend the Hello World `qml/Main.qml` (T111) into the app skeleton: `ApplicationWindow` fullscreen white, keep the stage `Item` of T116 scaled by `1/Screen.devicePixelRatio` but WITHOUT rotation (the system rotates, research R-15), `StackView` with all transitions removed, English strings through one `tr`-style function
- [x] T035 [P] [US1] Create `qml/pages/SetupPage.qml`: fields server URL, username, app password (masked), "Test connection" and "Save" buttons, result area showing success + CA source or the classified error (rejected credentials / server not reachable / certificate could not be verified / unsupported server / `http://` not allowed), text stating the password is stored unencrypted on the device and should be an app password
- [x] T108 [P] [US1] Create `qml/pages/SettingsPage.qml` (opened from a "Settings" button in the header of `qml/pages/NoteListPage.qml`, and registered in `qml/qml.qrc`): shows the CA source (`device` or `bundled`) and the custom CA status (`none`, `loaded` with the number of certificates, or `invalid` with a warning that the file is ignored), including the expected file location `custom-ca.pem` in the app data folder; no switch for verification exists (depends on T040)
- [x] T036 [US1] Extend the build wiring of T112: add `bridge/` to the `CMakeLists.txt` ARM branch (moc via `pbnotes_moc` for `notes_bridge.h` and the other bridge headers, qrc via `pbnotes_rcc` for `qml/qml.qrc` and the new `res/app.qrc`) and an optional host desktop app target when Qt6 dev packages exist; add the new QML pages to `qml/qml.qrc` and create `res/app.qrc` (depends on T003, T004, T032-T035)
- [x] T040 [US1] Make the CA path available to the app: the bridge calls `select_ca_file` with the default device candidates `/etc/ssl/certs/ca-certificates.crt` and `/etc/ssl/cert.pem` (the OpenSSL default locations in the SDK sysroot), extended by the results of T021 when available (this task does NOT wait for T021: the bundled file is the fallback), and the bundled path `/mnt/ext1/applications/pbnotes-cacert.pem` (host: `third_party/cacert/cacert.pem`), then `build_effective_ca` (T107) with `<data dir>/custom-ca.pem`, and passes the effective bundle path to `CurlTransport`; expose `caSource` and `customCaStatus` (`none`, `loaded`, `invalid`) as bridge properties (depends on T014, T028, T032, T107)
- [x] T041 [US1] Extend `INSTALL.md` (created by T113) with a section on trusting a private CA or self-signed server certificate by copying its certificate file to `/mnt/ext1/system/pbnotes/custom-ca.pem` (exact steps from T109)
- [x] T042 [MANUAL] [US1] Build with `POCKETBOOK_SDK` set, install the package on the device, and run quickstart step 5.2 (setup screen, connection test, CA source); record the outcome
- [x] T109 [MANUAL] [US1] Probe how users place `custom-ca.pem` on the device: confirm that a file copied to `system/pbnotes/custom-ca.pem` on the reader's USB mass-storage volume appears as `/mnt/ext1/system/pbnotes/custom-ca.pem`, whether the `system` folder is visible over USB, file-name case handling on the FAT partition, and whether the app restart picks the file up; record the exact steps in `research.md` R-6 and `INSTALL.md` (T041) **SKIPPED 2026-10-10 (decision of the maintainer): placing `custom-ca.pem` over USB was not tried on the device; the host tests cover the loading, and `system/pbnotes/` is known to be visible and writable over USB (R-6).**

**Checkpoint**: US1 works on host (tests) and on device (setup screen, connection test).

---

## Phase 4: User Story 2 - Read my notes, also offline (Priority: P1)

**Goal**: After setup, download all notes and read them in a large, e-ink-friendly reader, with the network off.

**Independent Test**: With sample notes on the server run the first sync, switch Wi-Fi off, open several notes; all display fully (quickstart 5.3). Host: pull integration tests and markdown tests pass.

### Tests for User Story 2 (write first, must FAIL)

- [x] T043 [P] [US2] Add fixtures `tests/fixtures/notes_list_full.json`, `tests/fixtures/notes_list_pruned.json` (unchanged notes with only `id`), `tests/fixtures/note_single.json` (anonymized)
- [x] T044 [P] [US2] Contract test `tests/contract/test_api_list.cpp`: `GET notes` with `pruneBefore` and `If-None-Match`; 200 parses notes and headers `ETag`, `Last-Modified`, `X-Notes-API-Versions`; 304 yields "unchanged"; 503 → `Maintenance`; invalid JSON → `Server`; `get_note` 200/404
- [x] T045 [P] [US2] Unit test `tests/unit/test_markdown.cpp`: headings (levels), bold, italic, bullet and numbered lists (nesting depth), task items `[ ]`/`[x]` with source line index, links rendered as their label text, unsupported constructs (tables, code blocks, images) remain plain text, HTML-special characters preserved as text
- [x] T046 [P] [US2] Unit test `tests/unit/test_store_pull.cpp`: `apply_remote` inserts unknown remote ids as `void`, updates a `void` note only if a field differs, leaves `edited`/`deleted` notes untouched, deletes `void` notes missing from the remote id list, keeps pending notes, stores collection ETag/Last-Modified and `api_versions`; the server's `title` is ignored (the title is always `derive_title(content)`)
- [x] T047 [P] [US2] Integration test `tests/integration/test_sync_pull.cpp`: `SyncEngine::run` (pull path) against `FakeTransport` for first pull (no stored ETag), incremental pull with `pruneBefore`, 304, 503, offline; unsupported API version (no `1.x`) makes no note request and returns `Unsupported`; reading after the transport is made to fail still works from the store
- [x] T048 [P] [US2] Benchmark test `tests/unit/test_perf_read.cpp`: with 1,000 generated notes of about 3 KB, `get` + markdown parse of one note completes well under the 3 s budget (SC-003)

### Implementation for User Story 2

- [x] T049 [US2] Implement `ApiClient::list_notes` and `get_note` in `core/src/api_client.cpp` (per `contracts/notes-api-v1.md`; timestamps are epoch seconds) (depends on T044)
- [x] T050 [US2] Implement `NoteStore::apply_remote`, `get`, `list` (unfiltered, sorted by modified desc), pull bookkeeping, and `excerpt` generation (first ~120 chars after the title) in `core/src/note_store.cpp` (depends on T046)
- [x] T051 [US2] Implement the pull half of `SyncEngine` in `core/include/pbnotes/sync_engine.h` and `core/src/sync_engine.cpp`: capabilities/version gate (require `1.x`, else `Unsupported`), single-flight guard, list with stored ETag/`pruneBefore`, apply results, treat 304/503/offline as non-errors with `Offline`/`Maintenance`/`Success` outcomes and update the account's `last_sync_*` fields (depends on T049, T050, T047)
- [x] T052 [P] [US2] Implement `parse_markdown` in `core/include/pbnotes/markdown.h` and `core/src/markdown.cpp` (blocks + spans per `contracts/core-library.md`; `toggle_task_item` is added in US6) (depends on T045)
- [x] T053 [US2] (done simpler: `listNotes()` returns a QVariantList, `openNote` returns styled HTML blocks directly in `notes_bridge.cpp`; no separate model classes, per Principle II) Extend the bridge: `notes` list model (`bridge/include/note_list_model.h`, `bridge/src/note_list_model.cpp`: localId, title, category, modified, favorite, pending, excerpt), `openNote(localId)` returning rendered blocks via `bridge/include/markdown_model.h` (spans converted to HTML-escaped styled text), `fontSize`/`fontPx` with `setFontSize` persisting setting `font_size` in {`small`, `medium`, `large`} default `medium`, first sync after `saveAccount` on a worker thread (depends on T032, T050, T051, T052)
- [x] T054 [P] [US2] Create `qml/pages/NoteListPage.qml`: large rows with title, category, modification date and a favorite marker; no highlight animation; touch targets about 8 mm; empty state text
- [x] T055 [P] [US2] Create `qml/pages/ReaderPage.qml`: non-interactive `Flickable` with page-up/page-down buttons and tap zones that set `contentY` instantly, block rendering for headings/emphasis/lists/task boxes (read-only here), links as plain text, font-size control (small/medium/large) persisted through the bridge, no animations
- [x] T056 [US2] Wire navigation in `qml/Main.qml`: setup page when `!bridge.configured`, else list → reader via `StackView` (no transitions); add `qml/pages/*.qml` to `qml/qml.qrc` (depends on T054, T055)
- [x] T120 [US2] Extend the bridge in `bridge/src/notes_bridge.cpp` (T032, T053) with `syncNow()` (pull-only in this story; push is added by T072/T073), properties `syncRunning`, `syncStatus` (time + outcome, never empty, built from the account's `last_sync_*` fields) and `syncOutcome`, and signal `syncFinished(outcome, message)`; the sync runs on a worker thread and a second request while one is running is ignored (depends on T051, T053)
- [x] T121 [P] [US2] In `qml/pages/NoteListPage.qml` (T054) show the sync status line (`bridge.syncStatus`) and a "Sync now" button that calls `bridge.syncNow()` and is disabled while `bridge.syncRunning`; after a failed first sync the empty list shows the status text and the button, so the user can retry without waiting for US4 (depends on T120, T054)
- [x] T057 [MANUAL] [US2] Device check of quickstart 5.3 (read offline, markdown rendering, font sizes, restart); record rendering/ghosting observations in `research.md` R-10

**Checkpoint**: US1 + US2 deliver a read-only offline notes viewer (suggested MVP demo).

---

## Phase 5: User Story 3 - Write and edit notes offline (Priority: P2)

**Goal**: Create, edit, delete (with confirmation), set category and toggle favorite offline with explicit save; changes persist and are marked pending.

**Independent Test**: Wi-Fi off; create, edit, categorize, favorite and delete notes; restart; all changes persist and are marked as not yet synced (quickstart 5.4).

### Tests for User Story 3 (write first, must FAIL)

- [x] T058 [P] [US3] Unit test `tests/unit/test_store_edit.cpp`: `create_local` rejects empty or whitespace-only content (FR-020); `save_local` sets status `edited`, new `modified`, recomputes title with `derive_title` and excerpt; `mark_deleted` removes the row if it has no `remote_id`, otherwise sets status `deleted`; toggling favorite and category are `edited` saves; pending notes survive store reopen
- [x] T059 [P] [US3] Unit test `tests/unit/test_category_validation.cpp`: category maximum 255 characters, no leading or trailing space, no control characters (U+0000-U+001F, U+007F); `/` is accepted and kept as typed; an invalid value keeps the previous category and returns an error key
- [x] T060 [P] [US3] Unit test `tests/unit/test_store_failure.cpp`: when a write fails (simulated disk-full via a failing VFS or read-only DB), the previous version of the note is preserved and the error is reported (edge case: device out of storage)

### Implementation for User Story 3

- [x] T061 [US3] Implement `create_local`, `save_local`, `mark_deleted` and category validation in `core/src/note_store.cpp` (atomic transactions; constraints quoted in T058/T059) (depends on T058, T059, T060)
- [x] T062 [US3] Extend the bridge with `loadForEdit`, `saveNote(localId, text, category, favorite)` (`-1` creates; empty new notes rejected), `deleteNote`, `setCategory`, `toggleFavorite`, properties `pendingCount`, signals `noteChanged`, `errorOccurred` in `bridge/src/notes_bridge.cpp` (depends on T061)
- [x] T063 [P] [US3] Create `qml/pages/EditorPage.qml` (layout and keyboard handling as validated by the T118 spike): plain `TextArea`, `Qt.inputMethod.show()` on open, explicit Save button, no timers, no live rendering, leave-with-unsaved-changes dialog (Save / Discard), Delete button with confirmation dialog, category prompt, favorite toggle
- [x] T064 [US3] Add "new note" button and edit/delete entry points to `qml/pages/NoteListPage.qml` and `qml/pages/ReaderPage.qml`; show a "not yet synced" marker for pending notes in the list (FR-010); update `qml/qml.qrc` (depends on T063)
- [x] T065 [MANUAL] [US3] Device check of quickstart 5.4 and the typing check 5.8; record keystroke latency and ghosting in `research.md` R-13 #2 and decide whether the `FullUpdate` fallback is needed

**Checkpoint**: Offline create/edit/delete works; changes wait for sync.

---

## Phase 6: User Story 4 - Keep notes in sync with the server (Priority: P2)

**Goal**: "Sync now" and sync at start with Wi-Fi: push local changes first, then pull; always show last sync time and status.

**Independent Test**: Make changes on device and server, tap Sync now, verify both sides match and the status line shows time and success (quickstart 5.5).

### Tests for User Story 4 (write first, must FAIL)

- [x] T066 [P] [US4] Add fixtures `tests/fixtures/note_created.json`, `tests/fixtures/note_updated.json` (with new `etag`/`modified`/`title`)
- [x] T067 [P] [US4] Contract test `tests/contract/test_api_write.cpp`: `POST notes` body has content, category, favorite, modified and NO title; `PUT notes/{id}` sends `If-Match` with the stored ETag unchanged; `DELETE notes/{id}` (404 treated as success; the conditional-GET sequence and header are tested in T102); response fields other than the title replace local ones (the title is always derived locally)
- [x] T068 [P] [US4] Integration test `tests/integration/test_sync_push.cpp`: push happens before pull (request order asserted); new note → POST and `remote_id` stored; edited note with remote id → PUT; deleted with remote id → conditional GET, then DELETE only on 304, then row removed (on 200 no DELETE is sent and the note stays pending); deleted without remote id → row removed without any request; a failing note does not stop the others and stays pending; outcome `Partial`
- [x] T069 [P] [US4] Integration test `tests/integration/test_sync_robustness.cpp`: transport failure injected at every request index leaves no lost or corrupted note (SC-005); a duplicate is accepted only when the failed request was a `POST` whose response was lost, and then both notes have identical text; a second `run` during a run returns "already running" without a second request; a note edited while its PUT is in flight stays `edited` (FR-036); 401 → `Failed/Auth` and local data untouched; TLS failure → `Failed/Tls`; offline → `Offline`; 503 → `Maintenance`; 304 → `Success`
- [x] T070 [P] [US4] Unit test `tests/unit/test_sync_status.cpp`: status text/state for never synced, running, success, partial, failed, offline, maintenance, with counts `{pushed, pulled, deleted, conflicts}` persisted in `last_sync_counts`
- [x] T102 [P] [US4] Contract test `tests/contract/test_api_delete.cpp` for the DELETE sequence (constitution Principle VI): the client first sends `GET notes/{id}` with `If-None-Match: <stored etag>`; on `304` it sends `DELETE notes/{id}`; on `200` (changed on the server) it sends NO `DELETE` and reports a conflict; on `404` it sends no `DELETE` and treats the note as already gone. Second variant (used only if T022 shows the server returns 412 for a stale `If-Match` on `DELETE`): the client sends `DELETE` with `If-Match` and no preceding `GET`, and a `412` answer is a conflict, not a deletion

### Implementation for User Story 4

- [x] T071 [US4] Implement `ApiClient::create_note`, `update_note` (with `If-Match`), `delete_note` in `core/src/api_client.cpp`, where `delete_note` performs the conditional GET with the stored ETag first and deletes only if the note is unchanged (or sends `If-Match` instead if T022 shows the server enforces it) (depends on T067, T102)
- [x] T072 [US4] Implement the push half of `SyncEngine` in `core/src/sync_engine.cpp`: per-note transactions, `confirm_push` in `core/src/note_store.cpp` keeping status `edited` if content/category/favorite differ from the sent snapshot, 404-on-PUT re-create with POST, deletes only after the unchanged check of T071 (a changed note is not deleted and stays pending until US5), status/count bookkeeping; a 412 response is NOT overwritten and the note stays `edited` until US5 adds conflict handling (depends on T068, T069, T071)
- [x] T073 [US4] Extend the sync members of the bridge from T120 for the push path: `pendingCount`, push counts in `syncStatus`; auto-sync at start — on device only when `QueryNetwork()` reports `NET_CONNECTED`, never calling `NetConnect*`; host always attempts; no error when offline at start (FR-024) in `bridge/src/notes_bridge.cpp` and `app/main.cpp` (depends on T070, T072, T120)
- [x] T074 [P] [US4] Promote the status line of T121 to a status bar in `qml/Main.qml` that is visible on the note list screen, the main view (SC-010): last sync time and outcome, pending count, "Sync now" button disabled while running
- [x] T075 [MANUAL] [US4] Device check of quickstart 5.5 and SC-009 timing with 1,000 notes; record results in `quickstart.md` notes

**Checkpoint**: Full read/write/sync loop works without conflict handling (data is never overwritten because `If-Match` is always sent).

---

## Phase 7: User Story 5 - Never lose a note to a conflict (Priority: P2)

**Goal**: When a note changed on both sides, keep both versions (server version stays, device version becomes an uploaded conflict copy) and tell the user.

**Independent Test**: Edit the same note on server and device (offline), sync: both texts exist, the copy is named as a conflict copy and also on the server, status reports the conflict (quickstart 5.6).

### Tests for User Story 5 (write first, must FAIL)

- [x] T076 [P] [US5] Add fixtures `tests/fixtures/put_412.json`, `tests/fixtures/note_server_changed.json`, `tests/fixtures/delete_404.json`
- [x] T077 [P] [US5] Integration test `tests/integration/test_sync_conflict.cpp`: PUT → 412 → exactly one `GET notes/{id}`, original replaced by the server version (status `void`), one new local note with the local text, category and favorite, status `edited` and no `remote_id`, pushed with POST in the same run, `conflicts` count 1; running again creates no further copy; a transport failure on the `GET notes/{id}` leaves the note untouched and `edited` and the next run retries; a failure of the copy's `POST` leaves the copy pending (nothing lost) and the next run uploads it; local and server versions identical in content, category and favorite → no copy, the original becomes `void`, `conflicts` stays 0
- [x] T078 [P] [US5] Unit test `tests/unit/test_conflict_title.cpp`: copy title is `<original title> (conflict YYYY-MM-DD)` written into the first line of the copy's content; if that title already exists locally append ` #2`, ` #3`, ...
- [x] T119 [P] [US5] Unit test `tests/unit/test_conflict_atomic.cpp` for `resolve_conflict` (Principle VI, SC-005): after success both rows exist, the copy's content equals the pre-conflict local content apart from the conflict marker appended to its title line (status `edited`, no `remote_id`) and the original equals the server version (status `void`); with a failure injected at every statement boundary of the transaction (test hook on the store's SQLite connection such as an authorizer or progress handler returning an error), reopening the store shows either the complete "before" state (original `edited` with the local text, no copy) or the complete "after" state, never the original replaced without the copy, and the local text exists in at least one row at every step; rerunning after a rollback produces exactly one copy; a UI edit made between the GET and the transaction ends up in the copy; when local and server versions are identical in content, category and favorite, `resolve_conflict` inserts no copy and only marks the original `void`
- [x] T079 [P] [US5] Integration test `tests/integration/test_sync_delete_edit.cpp`: PUT → 404 (deleted on server, edited locally) re-creates with POST and reports it; local delete of a note changed on the server: `GET` with `If-None-Match` returns 200 → server version restored as `void`, no DELETE sent, user informed; `GET` 304 → DELETE sent and row removed; `GET` 404 → row removed; if fixtures from T022 show `DELETE` honors `If-Match`, the test uses that path instead

### Implementation for User Story 5

- [x] T080 [US5] Implement `resolve_conflict(local_id, server_note)` and the conflict title rule in `core/src/note_store.cpp`: ONE SQLite transaction that (0) first checks whether the local and server versions are identical in content, category and favorite and, if so, only marks the original `void` (no copy, no conflict counted), otherwise (1) reads the current local row (so a concurrent UI edit is included), (2) inserts the conflict copy first (status `edited`, no `remote_id`, local content, category and favorite), (3) verifies inside the transaction that the copy's content equals the local content apart from the conflict marker appended to its title line, and only then (4) replaces the original with the server version (status `void`); any failure rolls back everything, so the local text is never present in zero rows (depends on T078, T119)
- [x] T081 [US5] Implement 412 handling, 404 re-creation reporting and the delete re-check in `core/src/sync_engine.cpp` per `research.md` R-8; the `GET notes/{id}` happens BEFORE and never inside the transaction, then `resolve_conflict` (T080) runs, then the copy is pushed with `POST` (depends on T072, T077, T079, T080)
- [x] T082 [US5] Extend the bridge with `conflictCount` and signal `conflictsDetected(count)` in `bridge/src/notes_bridge.cpp` (depends on T081)
- [x] T083 [P] [US5] Show conflict outcome in the status bar of `qml/Main.qml` ("N conflicts - copies kept") and a one-time dialog after a sync with conflicts or restored-deleted notes; conflict copies are ordinary notes found by title in `qml/pages/NoteListPage.qml`
- [x] T084 [MANUAL] [US5] Device check of quickstart 5.6 including delete-versus-edit in both directions; confirm the copy appears in the Nextcloud web UI

**Checkpoint**: No silent overwrite is possible in any tested scenario (SC-006).

---

## Phase 8: User Story 6 - Organize and find notes (Priority: P3)

**Goal**: Views (All, Favorites, Uncategorized, category), sort by modified or title, substring search, and toggling task-list checkboxes in the reader.

**Independent Test**: With notes in several categories and favorites, switch views, change sort, search a word that is only in a body, toggle a checkbox and sync (quickstart 5.7).

### Tests for User Story 6 (write first, must FAIL)

- [x] T085 [P] [US6] Unit test `tests/unit/test_store_views.cpp`: views `all`, `favorites`, `uncategorized`, `category:<name>` return the right notes (deleted notes excluded); sort `modified` newest first and `title` alphabetical; `categories()` lists non-empty categories with counts; setting values `view`, `sort_order` persist
- [x] T086 [P] [US6] Unit test `tests/unit/test_store_search.cpp`: case-insensitive substring match on title and content including non-ASCII letters; matches only the body word; works with all other filters; 1,000-note search completes under the 2 s budget (SC-008)
- [x] T087 [P] [US6] Unit test `tests/unit/test_markdown_toggle.cpp`: `toggle_task_item(text, line)` flips only `[ ]` ↔ `[x]` on that line, leaves every other byte unchanged, returns the text unchanged for a non-task line
- [x] T088 [P] [US6] Integration test `tests/integration/test_toggle_sync.cpp`: toggling saves the note as `edited` and the next sync pushes it with `If-Match`

### Implementation for User Story 6

- [x] T089 [US6] Implement view filters, sort, search and `categories()` in `core/src/note_store.cpp` (case folding for non-ASCII done in application code, not SQLite `LIKE`) (depends on T085, T086)
- [x] T090 [P] [US6] Implement `toggle_task_item` in `core/src/markdown.cpp` (depends on T087)
- [x] T091 [US6] Extend the bridge: `view`, `sortOrder`, `searchText`, `categories`, `setView`, `setSortOrder`, `setSearch`, `toggleTask` in `bridge/src/notes_bridge.cpp` (depends on T088, T089, T090)
- [x] T092 [P] [US6] Add a view picker (All, Favorites, Uncategorized, each category with count), sort control and search field to `qml/pages/NoteListPage.qml`; make task boxes tappable in `qml/pages/ReaderPage.qml`
- [x] T093 [MANUAL] [US6] Device check of quickstart 5.7 and SC-011 usability pass

**Checkpoint**: All six user stories are complete.

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Release-readiness, constitution compliance, documentation

- [x] T094 [P] Write `README.md`: purpose, prerequisites (cmake, ninja or make, `qt6-base-dev-tools`, curl dev package), host build + tests, device build (`POCKETBOOK_SDK` / `-DSDK_PATH`), packaging, install and `view.json` registration, a prominent note that the password is stored UNENCRYPTED in `pbnotes.db` on the device's FAT partition, recommended use of an app password and how to "Forget account", a "License: MIT" section pointing to `LICENSE`, `scripts/pre-merge.sh` as the merge gate, and the minimum tool versions (libcurl 7.85 or later, CMake 3.16, a C++17 compiler)
- [x] T095 [P] Create `scripts/check-constitution.sh` and run it in CTest: scans code AND shell scripts (`core/`, `bridge/`, `app/`, `tests/`, `scripts/` including `scripts/probe-device.sh`, `platform/`, `cmake/`, `CMakeLists.txt`) and fails on any of these exact forbidden forms: `curl -k`, `curl --insecure`, `CURLOPT_SSL_VERIFYPEER` or `CURLOPT_SSL_VERIFYHOST` set to 0, and `CURLOPT_SSL_VERIFYSTATUS`-style bypasses (`CURLOPT_SSL_VERIFYSTATUS`, `CURLOPT_PROXY_SSL_VERIFYPEER`, `CURLOPT_PROXY_SSL_VERIFYHOST`, `CURLOPT_DOH_SSL_VERIFYPEER`, `CURLOPT_DOH_SSL_VERIFYHOST`, `CURLOPT_DOH_SSL_VERIFYSTATUS` set to 0/false); also fails on `pocketbook-sdk` or `/opt/pocketbook` literals in build files, Qt/inkview includes under `core/`, note-content/password logging calls, or absolute local paths (user home directories on Linux or macOS, or drive-letter paths) in ANY tracked text file including `docs/`, `specs/`, `README.md` and `INSTALL.md` (device paths such as `/mnt/ext1` are allowed)
- [x] T096 [P] Edge-case tests in `tests/integration/test_edge_cases.cpp`: very long note (tens of thousands of characters) saves and parses, duplicate titles stay distinct, wrong device clock does not cause loss, category with `/` (accepted as typed), empty category (uncategorized), and 255+ character category per the validation rules
- [x] T097 [P] Host benchmark `tests/integration/test_perf_sync.cpp`: a simulated 1,000-note sync with a few changes completes quickly against `FakeTransport` (SC-009 host portion)
- [x] T122 [P] Create `scripts/pre-merge.sh` as the local merge gate (CI is deferred): configure and build the host target, run `ctest` (which includes the T095 constitution scan and the T123 fixture check), and run a secret scan (`gitleaks detect --no-git` if installed, otherwise built-in patterns for private-key blocks, `Authorization: Basic <base64>` values and non-empty `password = "..."` assignments outside `tests/`); exit nonzero on any finding (constitution Workflow: tests pass and no new secrets before merge)
- [x] T098 Document the MIT license (decided 2026-10-10): `LICENSE` exists with "Copyright (c) 2026 Volker von Einem" and `README.md` states "MIT License" and links `LICENSE`. The `NOTICE` file itself is created by T007 and MUST list only third-party items; re-check that no file claims a different license before the first public push
- [x] T099 Security review: confirm no secrets or real user data in the repository or fixtures, logs contain no personal data (T019), TLS never bypassed (T095), password forgotten correctly (T027); record findings in `docs/security-review.md`
- [x] T100 [MANUAL] Run the full `quickstart.md` on the device (steps 2-8) and SC-001, SC-003, SC-004, SC-007, SC-009, SC-010; update `research.md` R-13 with observed risks **PARTLY DONE, REST SKIPPED 2026-10-10 (decision of the maintainer): quickstart steps 2-7 were run on the device (R-16 to R-20); the typing check, the stopwatch criteria SC-001/SC-004, the untrusted-certificate check SC-007 on the device and the 1,000-note timing SC-009 were not measured (the host tests cover SC-007 and SC-009).**
- [x] T101 Update `docs/inkfeed-analysis.md` / `docs/pocketbook-sdk-analysis.md` references and `research.md` VERIFY items with confirmed facts after T021, T022, T042 **SKIPPED 2026-10-10 (decision of the maintainer): follows T022, which was skipped; the CA-store fact was recorded in R-6 (T021).**

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup & Hello World (Phase 1)**: none; start immediately. Ends with the device gates T114 (Hello World) and T118 (typing spike).
- **Foundational (Phase 2)**: after the Phase 1 skeleton tasks (T001-T009); host-only core work may run while T114 waits for the device. Blocks all stories. T021 and T022 are manual spikes: stories may start in parallel with them, but the TLS/CA and API-fixture tasks (T040, T022-dependent fixtures) must be revisited when results arrive.
- **US1 (P1)**: after Foundational and the Phase 1 device gates (T114, T118). Builds on the Hello World app, launcher and packaging.
- **US2 (P1)**: after Foundational; needs US1 only for account/setup UI and the shared bridge/app shell (the core pull path can be built and tested without it).
- **US3 (P2)**: after US2's store/list/reader (it edits notes shown there); core part independent.
- **US4 (P2)**: after US2 (sync engine pull) and US3 (pending changes to push).
- **US5 (P2)**: after US4 (push path) .
- **US6 (P3)**: after US2 (list/reader); independent of US3-US5 in the core; toggle uses the edit path from US3.
- **Polish**: after the desired stories; T098 gates the first public push.

### Manual-result dependencies

- T021 (device probe) refines the CA candidate list used by T040; T040 works with its default list and does not wait.
- T022 (live server) supplies the API fixtures; until then fixtures are hand-written and named `*.unverified.json`, and the VERIFY items in `research.md` stay open.
- T109 (custom CA placement) supplies the exact steps for T041.
- T114 and T118 are the Phase 1 device gates; only host-only core work may proceed without them.

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
2. The Phase 1 Hello World (T114) and typing spike (T118) prove the toolchain, host `rcc` output, Qt runtime, launcher, packaging and the TextArea/keyboard approach on the device; US1 then adds the TLS/CA decision (T042).
3. US2 → read-only offline viewer on the device. STOP and validate (T057).
4. Then US3 + US4 for the usable read/write/sync product, US5 for conflict safety. Do not release or sync real data before US5 (US4 never overwrites, but conflicting edits are only kept as unsent edits until US5).

### Incremental Delivery

Each phase ends at a checkpoint that is demonstrable on host (CTest) and on device (quickstart step). Commit after each task or logical group.

### Notes

- [P] tasks touch different files. Verify each test fails before implementing (Principle I).
- Task coverage of spec requirements: FR-001..005 (US1), FR-006, 011-014 (US2), FR-007-009 (US6), FR-010, 015-022 (US3), FR-023-031 (US4), FR-032-036 (US5), FR-037 and FR-031 are satisfied by not building the excluded features.
- Avoid: vague tasks, same-file conflicts, cross-story dependencies that break independence.
