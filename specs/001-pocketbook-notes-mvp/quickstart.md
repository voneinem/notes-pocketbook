# Quickstart: validating PocketBook Notes MVP

Validation guide only; implementation details live in `tasks.md`. Contracts:
[notes-api-v1](contracts/notes-api-v1.md), [core-library](contracts/core-library.md),
[qml-bridge](contracts/qml-bridge.md). Data: [data-model](data-model.md).

## 1. Prerequisites

Host (Linux x86-64):
- `cmake` >= 3.16 and a build tool (`ninja` or `make`); both exist in `/usr/bin` on the development
  machine and in `$SDK/bin`.
- A C++17 compiler (`g++`), `libcurl4-openssl-dev` (host libcurl for the core), `zip`.
- For the device build: Qt host `rcc` from `qt6-base-dev-tools` (Debian/Ubuntu), because the SDK's
  `rcc` cannot run here (needs ICU 76). The host `core` build and its tests do **not** need Qt.
- The PocketBook SDK; its location is supplied only through the environment:
  `export POCKETBOOK_SDK=<path to arm-buildroot-linux-gnueabihf_sdk-buildroot>`
  (or `-DSDK_PATH=...`). There is no built-in default.
- A Nextcloud test server with the Notes app enabled (API v1), a test user and an app password,
  reachable over HTTPS with a certificate trusted by the host (for the live checks in step 5).

## 2. Host build and tests (no Qt, no device)

```bash
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```
Expected: unit, contract and integration suites pass; no network is used (mocked transport and a
fake local server). Tests are written before the code they cover and fail first.

## 3. Device build (ARM)

```bash
cmake -S . -B build-arm -G Ninja -DTARGET_TYPE=ARM -DCMAKE_BUILD_TYPE=Release   # needs POCKETBOOK_SDK
cmake --build build-arm
file build-arm/pbnotes.app          # expect: ELF 32-bit LSB executable, ARM, EABI5
scripts/package-device.sh build-arm/pbnotes.app "" build-arm/dist
```
Expected: configuration fails with a clear message when neither `POCKETBOOK_SDK` nor `-DSDK_PATH`
is set; the zip contains `applications/pbnotes.app` (launcher), `applications/pbnotes.bin` (ELF),
`applications/icons/*.bmp`, `applications/pbnotes-cacert.pem`, `INSTALL.md`.

## 4. Device probe (run once on the InkPad One before relying on TLS or UI details)

Copy `scripts/probe-device.sh` to the device and run it (SSH via KOReader or the device's own
shell tool). It reports: Qt runtime version and `/usr/qml` contents, QPA plugins, candidate CA
store files and a handshake test, `QueryNetwork()` semantics, free space on `/mnt/ext1`.
Record results in `research.md` R-2, R-6, R-11 and resolve the VERIFY items.

## 5. Install and end-to-end scenarios on the device

1. Unzip onto `/mnt/ext1` (merging `applications/`), register in
   `system/config/desktop/view.json` as described in `INSTALL.md` (id `U_PbNotes`, absolute path,
   icons), reboot, start from Applications.
2. **Setup (US1)**: enter URL, user, app password → connection test succeeds and shows the CA
   source; wrong password, wrong host, `http://` URL, and an expired/self-signed certificate each
   show their distinct message and save nothing.
3. **Read offline (US2)**: first sync downloads all notes; switch Wi-Fi off; open notes with
   headings, emphasis, lists, links, task items; change font size; restart the app.
4. **Edit offline (US3)**: create, edit, set category, favorite, delete (confirm) with Wi-Fi off;
   restart; changes persist and show as pending.
5. **Sync (US4)**: Sync now → pushes first, then pulls; status line shows time and outcome;
   start the app with Wi-Fi on → automatic sync; with Wi-Fi off → no error.
6. **Conflicts (US5)**: edit the same note in the Nextcloud web UI and on the device offline, then
   sync → server text stays, device text appears as a conflict copy (also on the server), status
   reports one conflict; repeat for delete-vs-edit both ways.
7. **Organize (US6)**: switch All/Favorites/Uncategorized/category; sort; search a word that is only
   in a note body; toggle a task checkbox, sync, verify on the server.
8. **Typing check (risk R-13 #2)**: type ~500 characters in the editor; note latency and ghosting;
   record whether a refresh workaround is needed.

## 6. Success criteria spot checks

- SC-001: stopwatch for first-time setup < 3 min. SC-003: open a note < 3 s with 1,000 generated
  notes (host benchmark and device). SC-007: attempt a connection to a server with an untrusted
  certificate → refused, credentials not sent (verify with the server's access log). SC-009: sync
  of 1,000 notes with a few changes < 30 s.

## 7. Reference

Review `research.md` R-13 (risks) after every device run and update the VERIFY items.
