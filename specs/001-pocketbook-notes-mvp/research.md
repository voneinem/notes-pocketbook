# Research: PocketBook Notes MVP

Sources: `docs/inkfeed-analysis.md`, `docs/pocketbook-sdk-analysis.md`,
`docs/notes-android-features.md` (behavior only), the SDK headers, and the constitution.
Items marked **VERIFY** cannot be settled from this machine and become tasks (device probe or live
server check). Nothing here required a new `NEEDS CLARIFICATION` in the plan.

## R-1. Build structure: ARM vs host, SDK wiring

- **Decision**: One top-level `CMakeLists.txt`. `-DTARGET_TYPE=ARM` selects the cross toolchain
  inline (before `project()`), anything else is a host build. On ARM the SDK path is read from
  `-DSDK_PATH`, else from the environment variable `POCKETBOOK_SDK`; if neither is set,
  configuration fails with a message. There is no default path.
- **Rationale**: Matches InkFeed (`CMakeLists.txt:9-25`) and the constitution (SDK location only
  from env var / `-DSDK_PATH`). InkFeed's default path contains a `sdk_extracted/` level that does
  not exist locally (`docs/pocketbook-sdk-analysis.md` §7.2), which shows hard-coded defaults rot.
- **Alternatives**: Use the SDK's `share/buildroot/toolchainfile.cmake` (`-DCMAKE_TOOLCHAIN_FILE`).
  Rejected for now: its system name is `Buildroot` and it sets `QT_HOST_PATH`, which changes how
  Qt helpers behave; the inline Linux/ARM setup is proven by InkFeed. Revisit if a plain
  `find_package(Qt6)` cross build is ever wanted.
- **Notes**: `cmake` and `ninja` exist on this host (`/usr/bin`) and in the SDK (`$SDK/bin`). The
  host has no Qt `rcc` (R-3). `relocate-sdk.sh` was never run and the compiler works anyway.

## R-2. Qt on the device vs the SDK (risk)

- **Decision**: Build against the SDK's Qt 6.8.2 and link by absolute `.so` path only
  `Quick, Qml, Gui, Core` (+ `Concurrent` if used), as InkFeed does. `QtQuick.Controls` is
  resolved at runtime from the device's `/usr/qml`.
- **Rationale**: Proven pattern. The launcher points the loader at the device's own Qt in
  `/usr/lib`.
- **Risk**: The device's runtime Qt minor version was never checked. A mismatch (for example 6.5
  on the device) could break QML imports or the ABI. **VERIFY** with `scripts/probe-device.sh`
  (reads the version of `/usr/lib/libQt6Core.so.*`, lists `/usr/qml`, `/usr/plugins/platforms`).
  Mitigation: keep QML to basic `Item/Text/TextArea/TextField/Flickable/ListView/StackView/Button`
  features, avoid Qt 6.8-only QML APIs, and pin the minimum required version in the README.
- **Alternatives**: Bundle Qt with the app. Rejected: large, high effort, and the QPA plugin is
  the device's.

## R-3. Resource compiler and moc

- **Decision**: Run `moc` from `$SDK/local/qt6/libexec/moc` (6.8.2, verified to run) and `rcc` from
  the host's Qt tools package (`qt6-base-dev-tools` on Debian/Ubuntu) with `--no-zstd`, both via
  explicit `add_custom_command`, as InkFeed does. QML is loaded from the qrc at runtime (no
  `qmlcachegen`).
- **Rationale**: The SDK's `rcc` fails here (`libicui18n.so.76` missing, verified). `--no-zstd`
  avoids a compressed-resource symbol the sysroot Qt does not export.
- **Risk**: Host `rcc` is Qt 6.10.2 while the runtime is 6.8; qrc binary format has been stable
  across Qt 6.x, but this is the author's claim in InkFeed, not verified here. Mitigation: a
  device smoke test in the quickstart (app starts and loads `qrc:/qml/Main.qml`); keep the
  host rcc version printed at configure time; if it fails, fall back to `rcc --format-version`
  options or to a 6.8 host rcc from the Qt online installer.
- **Alternatives**: `AUTORCC` (needs a matching host toolchain). Embedding QML as C++ strings.
  Both rejected as more complex.

## R-4. Layering and threads

- **Decision**: `notes_core` is a static library with no Qt, no InkView. It exposes synchronous
  C++ interfaces (`SyncEngine::run()` blocks). The bridge runs sync on a worker thread
  (`std::thread` or `QtConcurrent::run`) and posts results to the UI thread with queued signals.
  The store serializes access with one mutex around a single SQLite connection.
- **Rationale**: Satisfies constitution V; keeps tests deterministic. One connection plus a
  mutex is the simplest model that is safe with vfat.
- **Alternatives**: A connection per thread (more locking edge cases on vfat); an async core
  (callbacks everywhere, harder to test).

## R-5. HTTP layer and mocking

- **Decision**: An abstract `HttpTransport` (request: method, URL, headers, body; response:
  status, headers, body, transport error class). `CurlTransport` implements it; tests use a
  scripted `FakeTransport` and an in-process fake HTTP server for end-to-end sync scenarios.
- **Rationale**: Constitution IV/V and the plan input ("mocked HTTP"). curl is in the sysroot
  (8.16.0) and on the host.
- **Details**: connect timeout 10 s, total timeout 60 s per request, `NOSIGNAL`, `ACCEPT_ENCODING`
  gzip, `FOLLOWLOCATION` limited to https (`CURLOPT_REDIR_PROTOCOLS_STR=https`), `PROTOCOLS_STR=https`,
  Basic auth via `CURLOPT_USERPWD` (curl does not forward credentials to other hosts on redirect),
  header `OCS-APIRequest: true` for OCS calls.
- **Alternatives**: InkView `Download()` (observed hanging per InkFeed notes), `QNetworkAccessManager`
  (Qt dependency in core; TLS behavior on device unverified).

## R-6. TLS and CA store

- **Decision**: Always `VERIFYPEER=1`, `VERIFYHOST=2`, no setting or env var turns this off. CA
  file selection at runtime (`ca_selector`): try the device store candidates in order
  (`/etc/ssl/certs/ca-certificates.crt`, `/etc/ssl/cert.pem`, further paths from the device probe)
  and accept one only if it is a readable, non-empty file containing at least one PEM certificate;
  otherwise use the bundled CA file (repository `third_party/cacert/cacert.pem`, installed on the device as `/mnt/ext1/applications/pbnotes-cacert.pem`). The chosen path is set with
  `CURLOPT_CAINFO` and logged (path only). The connection test reports which source is used.
- **Rationale**: Constitution III says device store when usable, else bundled CA file. InkFeed
  found no CA bundle and disabled verification; we must not copy that. The SDK sysroot has an empty
  `etc/ssl/certs`, and OpenSSL's default dir is `/etc/ssl`, so the device's real content is unknown.
- **Device result (2026-10-10)**: the device's own CA store is usable: the app selected
  `source=device` on every start and the TLS connection to a real hosted Nextcloud server was
  verified with it, with no custom CA. The bundled file stays as the fallback for other firmware.
- **Bundled file**: Mozilla bundle from curl.se, pinned by date and SHA-256, MPL-2.0, attributed in
  `NOTICE`; refreshed by a documented manual step, with the age shown in the About text.
- **Custom CA (user-supplied)**: if `custom-ca.pem` exists in the app data directory
  (`/mnt/ext1/system/pbnotes/`), `build_effective_ca` concatenates the base CA file chosen above
  (device store if usable, otherwise the bundled file) with it into `effective-ca.pem` and that file
  is used as `CURLOPT_CAINFO`. The custom file must be a readable, non-empty PEM file with at least
  one certificate and at most 1 MB; otherwise it is ignored, the base is used, and Settings shows
  "invalid". Verification always stays on and no setting trusts unverifiable certificates. A
  self-signed server certificate works by placing that certificate in the file. Settings shows
  whether a custom CA is loaded and how many certificates it contributes (counted from the PEM
  markers), so the user can see that the file was read. The server's host name must still match
  its certificate, so a self-signed certificate needs a correct subject alternative name.
- **Why the base is "device store if usable, otherwise bundled"** (not always the bundled file):
  decided 2026-10-10. Principle III already requires this order; the device store follows firmware
  updates and may already hold a CA the user installed; and the selection code (T014) is needed
  anyway. Always using the bundled file would be more predictable across devices, but it would
  ignore the device store and need a constitution change, so it was rejected. The effective bundle
  is the chosen base plus `custom-ca.pem`.
- **VERIFY (device)**: how a user copies `custom-ca.pem` onto the device (USB mass storage to
  `system/pbnotes/`, file-name case on FAT, visibility of the `system` folder) is checked by task
  T109 and documented in `INSTALL.md`. Seen so far: the `system/pbnotes/` folder (database, logs)
  is visible and writable over USB mass storage; placing a real `custom-ca.pem` has not been tried.
- **Self-signed trust option**: the in-app "trust anything" or per-server trust toggle is excluded
  from this release (spec FR-003); `custom-ca.pem` is the supported way.
- **Also**: only `https://` server URLs are accepted; `http://` is rejected in setup with an
  explanation, since Basic auth over plain HTTP would expose the app password.

## R-7. Notes API v1 usage

- **Decision**: Base `/index.php/apps/notes/api/v1/` (path prefix configurable only through the
  server URL, so sub-directory installs work). Capabilities via
  `/ocs/v2.php/cloud/capabilities?format=json` with Basic auth and `OCS-APIRequest: true`; read
  `ocs.data.capabilities.notes.api_version` and fall back to the `X-Notes-API-Versions` header of the
  first notes call. Require version `1.x` (`1.0` is the minimum, major `1`); if absent, show
  "This server does not support the Notes API version 1" and do not sync.
- **Operations**: list with `GET notes?pruneBefore=<epoch s>` and `If-None-Match: <collection ETag>`;
  create `POST notes`; update `PUT notes/{id}` with `If-Match: "<note etag>"`; delete
  `DELETE notes/{id}`; read one `GET notes/{id}`. Timestamps are epoch seconds.
- **Request bodies**: content, category, favorite, modified (seconds). The title is **not** sent;
  the server derives it from the first line (spec FR-019). After each push the returned note
  replaces the local etag and modified time; the returned title is stored as the display title of the now-synced note (spec FR-019); the app never sends a title.
- **VERIFY (live server, contract test fixtures)**: (a) the server accepts `If-Match` on `PUT`
  and returns `412` with the current ETag semantics; (b) how the server derives the title from the
  first line (markdown `#` stripping, characters replaced) so the local preview title matches;
  (c) whether `DELETE` honors `If-Match` (see R-8, fallback below); (d) behavior of `pruneBefore`;
  (e) `chunkSize` / `chunkCursor` pagination support for large first pulls. Fixtures recorded from
  a throwaway test server and anonymized.
- **Rationale**: Behavior mirrors `docs/notes-android-features.md` §5-6, plus the conditional
  writes that the Android app lacks (that doc §6.4). Using API v1 only removes the compatibility
  layer.
- **Alternatives**: Support v0.2 too (rejected: extra code for old servers, YAGNI).

## R-8. Sync algorithm (with conflicts)

Order: **push first, then pull**. One run at a time (mutex + state flag); a second request while
running returns "already running".

Push, per pending note, each in its own transaction:
- `edited`, no remote id: `POST`; store remote id, etag, server title, modified; set `void`.
- `edited`, remote id: `PUT` with `If-Match` = stored etag.
  - 200: replace with response, `void` (only if local content/category/favorite are unchanged since
    the request began; otherwise keep `edited` so a concurrent UI edit is not lost).
  - **412 (conflict)**: fetch the server version with `GET notes/{id}` (network only, no database
    change; if it fails the note stays `edited` and the next run retries). Then ONE database
    transaction: read the current local row, insert the conflict copy first (local text, category
    and favorite, title with conflict marker and date, status `edited`, no remote id), verify the
    copy's content equals the local content apart from the conflict marker on its title line, and only then replace the original with
    the server version (`void`). Any failure rolls the whole transaction back, so the local text is
    never in zero rows. After the commit the copy is pushed with `POST` (same run, or the next run
    if that fails). Count one conflict. If the local and server versions are identical (content,
    category, favorite), no copy is made: the original just becomes `void` and no conflict is counted.
  - 404: the note was deleted on the server; re-create with `POST` (spec FR-035), clear the old
    remote id. Count as informational "recreated".
- `deleted`, no remote id: remove the row.
- `deleted`, remote id: conditional re-check first: `GET notes/{id}` with `If-None-Match` = stored
  etag. `304`: send `DELETE`, then remove the row. `200` (server changed): restore the server
  version locally as `void`, inform the user (spec FR-035). `404`: remove the row. If **VERIFY (c)**
  shows that the server returns 412 for a stale `If-Match` on `DELETE`, send the header and drop
  the extra `GET`. Otherwise a small race remains between the check and the delete; mitigation is
  the server's trash bin where enabled (recorded in plan Complexity Tracking).
- A failing note does not stop the others; its status stays pending.

Pull:
- `GET notes?pruneBefore=<stored Last-Modified>` with `If-None-Match` = stored collection ETag.
  `304`: nothing changed. Otherwise apply each returned note: unknown remote id → insert `void`;
  known id with status `void` and a differing field → update; known id with pending local
  status → leave (it will be reconciled at the next push). Notes whose id is absent from the
  response and whose local status is `void` are deleted locally; pending notes survive.
- Store the response ETag and Last-Modified; store the API version list.
- First pull (no stored ETag): same call without `pruneBefore`; if the server supports chunking
  (VERIFY e), page with `chunkSize=100`.

Results and errors:
- `304`, `503` (maintenance) and no-network are **not errors** for the data; the status line shows
  "Up to date", "Server in maintenance, try later" and "Offline, not synced" respectively. This
  deviates from the Android app, which hides offline results, because the spec requires a visible
  status at all times (FR-027).
- `401/403`: "Credentials rejected" and stop; local data untouched.
- TLS failure: "Certificate could not be verified" and stop; no credentials are sent.
- Outcome values: `never`, `running`, `success`, `partial` (some pending notes failed),
  `failed`, `offline`, `maintenance`.
- Favorite toggle and checkbox toggle are normal local edits (status `edited`).
- **Known limitation (lost POST response)**: if the server created a new note but the response
  never reached the device (connection drop, app killed), the note still has no remote id and the
  next sync POSTs it again, so it exists twice on the server with identical text. Nothing is lost.
  Automatic matching was rejected as extra complexity for a rare, safe failure (decided
  2026-10-10); revisit if it occurs in practice.

## R-9. Markdown rendering (Qt-free)

- **Decision**: A small block/span parser in `notes_core` (headings, bullet/numbered lists, task
  items, paragraphs; spans for bold, italic; links shown as their text). It returns blocks with
  source line offsets so `toggle_task_item(text, line)` edits the note text exactly. The bridge
  turns spans into a `Text.StyledText`-safe string (all content HTML-escaped).
- **Rationale**: Host-testable, deterministic toggling, no dependency on `cmark` or Qt's markdown
  support (which gives no source mapping and cannot be host-tested without Qt).
- **Alternatives**: `Text.MarkdownText` (Qt-only, no checkbox write-back); vendored cmark-gfm
  (new dependency, YAGNI).

## R-10. UI patterns for e-ink

- **Decision**: Single `ApplicationWindow` + `StackView` with no transitions; stage item scaled by
  `1/devicePixelRatio` and NOT rotated: rotation is done by the system. On a sensor change the app
  calls InkView `SetOrientation(n)` and then the platform screen's `updateGeometry()`, with the
  keyboard closed around the change (measured on the InkPad, see R-15; the InkFeed pattern of
  rotating the content inside a portrait window leaves the system keyboard and status bar in
  portrait and was rejected). The rotation preference (auto/portrait/landscape) is stored in
  settings. The reader
  uses a non-interactive `Flickable` with page-up/page-down controls and tap zones that jump
  `contentY` instantly (no flick physics, no scroll animation). The editor uses `TextArea` and
  `Qt.inputMethod.show()`; cursor blinking is disabled (`QStyleHints::setCursorFlashTime(0)`)
  to avoid periodic panel refreshes. Lists use `ListView` with large rows (touch targets of about
  8 mm) and no highlight animation.
- **Rationale**: Page jumps and a static cursor reduce refreshes and ghosting. InkFeed found no
  e-ink refresh API usage and relies on the QPA plugin (`docs/inkfeed-analysis.md` §6).
- **Device result (2026-10-10)**: see R-15. Still to confirm with the real editor: ghosting over a
  long typing session, and whether a periodic full refresh is needed (`FullUpdate` behind a
  Settings action stays the fallback).

## R-11. Network availability at start

- **Decision**: On device, check `QueryNetwork()` from `inkview.h` (`NET_CONNECTED` mask) once at
  start; if connected, start a sync; if not, do nothing and show "Offline, not synced". The app
  never calls `NetConnect*` on its own (no dialogs at launch). On the host, always attempt.
- **Rationale**: Matches FR-024 (no error when Wi-Fi is not available at start).
- **VERIFY**: exact return-value semantics of `QueryNetwork()` on the device (probe script).

## R-12. Test tooling

- **Decision**: Unit/contract/integration tests are plain C++17 executables registered with CTest,
  using a tiny in-repo assertion header (no external framework) or a single-header framework
  chosen when `tasks.md` is generated. The SDK ships GoogleTest/GoogleMock 1.15 for the target, but
  those are ARM builds and not usable on the host.
- **Rationale**: Keep dependencies minimal (constitution II) and the host build independent of the
  SDK.
- **Alternatives**: Vendored doctest/Catch2 (acceptable later; decide with the first task).

## R-13. Risks (consolidated)

| # | Risk | Impact | Mitigation / detection |
|---|------|--------|------------------------|
| 1 | Device Qt version differs from the SDK's 6.8.2 | App fails to start or QML import errors | Probe script; restrict QML features; log Qt runtime version at start; document minimum version |
| 2 | Text editing latency and ghosting on e-ink | Poor typing experience, user abandons editor | Plain `TextArea`, no live markdown, no cursor blink, no per-keystroke work (no autosave, no live word count); measure keystroke-to-paint on device (the Phase 1 typing spike T118 tests this before US3 starts); fallback: edit in a dialog with delayed repaint, `FullUpdate` action |
| 3 | Host Qt 6.10 `rcc` output vs Qt 6.8 runtime | qrc fails to load | `--no-zstd`; smoke test on device; fallback to a 6.8 host rcc |
| 4 | Password stored unencrypted in the DB on vfat (readable over USB mass storage by anyone with the device or its backups) | Credential exposure if the device is lost or shared | Use a revocable Nextcloud **app password** only; document the risk in README and setup screen text; never log or display it; never export it; provide "Forget account" that overwrites the stored value (`secure_delete=ON`, backup file removed after migrations, verified by the T027 byte-scan test); no OS keystore exists on the device (VERIFY) |
| 5 | Notes API behaviors assumed (If-Match on PUT/DELETE, title derivation, chunking) | Wrong conflict handling | Contract tests with recorded fixtures; live-server verification task; fallbacks in R-8 |
| 6 | No usable device CA store | Setup always fails on HTTPS | Bundled CA file ships regardless; CA source shown in the test result |
| 7 | vfat: weak locking, no atomic rename guarantees, power loss | DB corruption | `journal_mode=TRUNCATE`, `synchronous=FULL`, short transactions, integrity check at start with automatic backup copy of the DB before schema migrations |
| 8 | Host lacks Qt `rcc`; SDK `rcc` unusable; SDK never relocated | Cannot build | Quickstart lists prerequisites (`qt6-base-dev-tools`, cmake, ninja or SDK's); core and tests build without Qt |
| 9 | Server certificate expires or uses a private CA | Sync refused | Clear message; local use continues; private CAs and self-signed certificates are supported through `custom-ca.pem` (R-6) |
| 10 | Large first pull (many notes) on slow Wi-Fi | Long sync, timeouts | Chunked listing if available; progress count in status; resumable (each note stored as it arrives) |
| 11 | Title derivation mismatch between client and server | The server's title can differ from the local derivation | Notes in sync show the server title (verified on a real server: titles are file names and differ from the first line after renames/cleanup); notes with local changes show `derive_title`; a one-time full pull refreshes titles stored by older versions |

## R-14. Naming and licensing mechanics

- **Working name**: `pbnotes` (binary `pbnotes.bin`, launcher `pbnotes.app`, data dir
  `/mnt/ext1/system/pbnotes/`, menu id `U_PbNotes`). It is a placeholder constant in one CMake
  variable and the packaging script, to be replaced when a product name is chosen.
- **InkFeed (MIT)**: the launcher, packaging flow, CMake ARM block and QML patterns are adapted,
  not copied verbatim where avoidable; `NOTICE` lists InkFeed's copyright and license text for
  the adapted patterns.
- **notes-android (GPL-3.0)**: nothing copied; sync behavior is reimplemented from
  `docs/notes-android-features.md` and the public Notes API.
- **Project license**: MIT (decided 2026-10-10; `LICENSE` at the repository root, copyright holder
  Volker von Einem). The README statement is required before the first public push (task T098).
  MIT is compatible with using InkFeed (MIT) patterns; the GPL-3.0 notes-android stays reference-only.
- **Packaging/registration**: zip layout as InkFeed (`applications/pbnotes.app`,
  `applications/pbnotes.bin`, `applications/icons/*.bmp`, `INSTALL.md`, bundled `pbnotes-cacert.pem`);
  user registers the app in `system/config/desktop/view.json` (id starts with `U_`, absolute path,
  opaque 122x122 8-bit BMP icons) and reboots. GitHub Actions is deferred.

## R-15. Device results: Hello World and typing spike (2026-10-10)

Measured on the user's InkPad One with the Phase 1 Hello World app (gates T114 and T118). Raw logs
were reviewed from `system/pbnotes/hello.log` and `pbnotes.log` on the reader.

| Topic | Result |
|-------|--------|
| Build and start (T114) | The ARM build packaged by `scripts/package-device.sh` installs and starts from the Applications menu; Quit exits with code 0 and returns to the launcher cleanly |
| Qt runtime (R-2) | The device runs Qt **6.10.3**; the app built against the SDK's 6.8.2 and resources made with host `rcc` 6.10.2 `--no-zstd` run without problems |
| Screen | Qt screen 1080x1440 logical at devicePixelRatio 1.30003 (panel 1404x1872); window 1080x1335 below the status panel |
| Keyboard (R-13 #2) | The on-screen keyboard is the reader's native one (`OpenKeyboard`, latinime German/English suggestions). It opens on `Qt.inputMethod.show()`, keys hit what is tapped, the text field stays visible, in portrait and in both landscape directions |
| Typing speed | App-side time from a keystroke to the next frame: 45 ms on average, 50 ms at most (26 samples). Panel ghosting was not judged over a long session |
| Rotating the content only (InkFeed pattern) | Content rotates, but the system keyboard and status bar stay portrait: rejected |
| `SetOrientation(n)` only | The system presents the content upright and rotates keyboard and status bar, but Qt's window stays portrait-sized (the layout fills only part of the landscape screen); no Qt geometry event is emitted |
| `SetOrientation(n)` + platform screen `updateGeometry()` | **Works in all four orientations.** Qt screen 1080x1440 <-> 1440x1080, window 1080x1335 <-> 1440x975; the layout, touch positions, keyboard and status bar are correct. The platform screen (`EinkPlatformScreen`, a QObject) is reached with `QGuiApplication::primaryScreen()->handle()`, `dynamic_cast<QObject*>` and `QMetaObject::invokeMethod(..., "updateGeometry")` (private QPA header used only for the cast). Calling `setOrientation(int)` on it as well is not needed. Do NOT rotate the content on top of this (double rotation) |
| Rotating with the keyboard open | The keyboard stays in the old orientation and the screen updates only partly. **Fix verified:** close the keyboard, `SetOrientation`, `updateGeometry` (about 0.4 s later), forced redraw, reopen the keyboard (about 1.4 s in total) |
| Wi-Fi API (R-11) | `QueryNetwork()` returned 515 (0x203, Wi-Fi ready) when connected and 3 (Wi-Fi on, not connected) otherwise: connected iff `(value & 0x0f00) != 0` |
| Hardware keys | The reader has no hardware Back button; Back is an on-screen button. A `Key_Enter` from the keyboard arrives as Qt key 0x1000005. The page stack must provide its own Back |
| Back button bug found | `StackView.view` is only valid on a page's root item, not inside a child's handler (fixed in the spike) |

### R-16. Device results: setup screen (User Story 1, 2026-10-10)

| Topic | Result |
|-------|--------|
| First UI build | Showed a blank screen: `StackView.initialItem` with a URL expression silently loads nothing. Fixed with `push()` in `Component.onCompleted`; the `--self-test` now fails when no page is shown |
| CA store (R-6, T021) | The reader's own CA store works: a connection test against a public Nextcloud host (verified certificate, `ca.selected source=device`) succeeded with certificate verification on. The bundled CA file is therefore only a fallback |
| Wi-Fi off | With Wi-Fi off the name lookup fails (`curl 6: Could not resolve hostname`). The app now checks `QueryNetwork()` and, for a user-initiated test, asks the system to connect with `NetConnectAsync` (it may show its own dialog); verified: connects and runs the test, or reports "Wi-Fi is not connected" |
| Setup flow | Empty/`http://`/unreachable inputs give the right messages and keep Save disabled; successful test enables Save; Save leads to Home; the app starts on Home afterwards |
| Settings | Certificate status ("the reader's own", custom CA none), rotation preference cycle (auto/portrait/landscape) and Forget account with confirmation (cancel keeps the account, confirm returns to an empty setup form) work |
| Draft | Entries typed on the setup screen are kept when a test fails and restored on the next start (no retyping of long URLs/app passwords); cleared by Save and Forget account |

### R-17. Device results: note list, reader, sync (User Story 2, 2026-10-10)

| Topic | Result |
|-------|--------|
| First sync | 12 notes pulled after Save; "Last sync" line with counts; times shown in the C library's local time (Qt's own time zone lookup gave a wrong time on the device) |
| Sync now with Wi-Fi off | `NetConnectAsync` brings Wi-Fi up (system dialog) and the sync then runs; if it fails the user is told to turn Wi-Fi on |
| Rendering | Headings, bold/italic, lists, drawn checkboxes and the favorite star render correctly; no animations, scrolling by page buttons and edge taps (invisible 50 px strips) works; no smearing noticed |
| List paging | Moves by whole rows (a fixed pixel step cut rows in half) |
| Empty notes | Two notes had no text on the server (only a file name as title): they showed blank rows. Now: server title as display title |
| Titles | The server's titles differ from the first line (renames, cleanup of `#`, `:`, `/`); decision: notes in sync show the server title, notes with local changes the first line (FR-019 changed) |
| Offline reading | Works with Wi-Fi off after a restart; no waiting on the network |
| Font size | small/medium/large with A-/A+ persists across restarts |

Not yet verified: caret blinking, non-ASCII input, selection and paste, and
ghosting over a long session.

### R-18. Device results: pushing changes, start-time sync, conflict (User Story 4, 2026-10-10)

| Topic | Result |
|-------|--------|
| Push | A new note, an edit and a delete made on the reader reach the server on "Sync now"; the status line shows "N sent" and the pending count disappears |
| Pull | An edit made on the server arrives on the reader ("1 received") |
| Start | With Wi-Fi on the app syncs by itself at start; with Wi-Fi off it starts without an error and does not switch Wi-Fi on; notes stay readable |
| Conflict | A note edited on both sides keeps both versions: the server's text replaces the note, the reader's text is saved as a copy titled "... (conflict YYYY-MM-DD)" and uploaded; the user is told |

Not measured on the device: SC-009 timing with 1,000 notes (the host benchmark covers the algorithm).

### R-19. Device results: conflict handling (User Story 5, 2026-10-10)

| Topic | Result |
|-------|--------|
| Edited on both sides | Dialog "Notes changed in two places" and status "1 conflict(s) - copies kept"; the server version keeps the title, the reader text is kept as a "(conflict date)" copy that also appears on the server |
| Deleted here, edited on the server | The note is not deleted and returns with the server text; status "1 deleted note(s) restored" |
| Edited here, deleted on the server | The edited note is uploaded again as a new note; nothing is lost |
| No change | A further sync shows no dialog |

### R-20. Device results: views, sort, search, task boxes (User Story 6, 2026-10-10)

| Topic | Result |
|-------|--------|
| View picker, sort, search | All, Favorites, Uncategorized and categories with counts; sort newest / A-Z; search in title and body (combined with the view, 0.5 s after the last key); view and sort are remembered, the search text is not; typing with the system keyboard is acceptable |
| Task boxes | `- [ ] item` lines are drawn as boxes and flip with one tap, the scroll position stays, the change is pending and arrives on the server. Lines such as `[ ] item` (no leading dash) are not task items, in the web and Android apps either |
| Fixed after the check | The View button kept the old category name after the category ran out of notes (the property did not announce the change) |

