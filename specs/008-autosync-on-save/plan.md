# Implementation Plan: Autosync on Save

**Branch**: `feature/008-autosync-on-save` | **Date**: 2026-10-12 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/008-autosync-on-save/spec.md`

## Summary

A Settings switch "Auto Sync" (on by default). When it is on, every completed local change
to a note (save, delete, favorite, category, task checkbox, attachments saved with a note) arms a
small scheduler; when its rules say so, the app starts the **existing** sync. Nothing about the sync
itself changes. Built on the existing layers (Qt-free core, thin Qt bridge, QML) with **no new
library, no schema change and no server change**:

- **Decision logic in the core, host-tested.** A small Qt-free `SyncScheduler` (`pbnotes/sync_scheduler.h`)
  holds the rules and gets the time and the situation as arguments: *quiet period* after the last
  save (4 s) so close saves combine; *minimum gap* between two sync starts (15 s) so a stream of
  saves is not one sync per save; *one follow-up* when a save lands while a sync runs (the running
  sync may already have read the pending list); *drop, don't retry* when offline, when nothing is
  pending any more, when the switch is off or there is no account; *defer* while an editor page is
  open. Tests drive it with plain numbers, no timers and no sleeping.
- **The bridge only wires it.** `NotesBridge` calls `note_saved` from the six places that change a
  note, asks the scheduler what to do, and starts `syncNow()` or arms one single-shot `QTimer` for
  the wait the scheduler returned. The Wi-Fi check is an injected callback, the same pattern as
  `AttachmentsBridge::Config::wifiConnected`; the device build passes `DeviceBridge::wifiConnected`.
  When Wi-Fi is off it asks the device to connect, once per save, as "Sync now" does (revised after the first device test: with the reader's usual Wi-Fi-off state the first design did nothing).
- **The preference is one setting row** (`sync_after_save`: `on`/`off`, default `on`) in the
  existing key/value settings table, so there is no schema change and it survives "Forget account".
- **A safety guard the feature needs.** A background sync can now be running while the reader opens
  another note in the editor. A pull may replace that note's text on the device with a newer server
  version; saving the editor's older text would then be pushed with the *new* ETag and overwrite the
  server's version without a conflict (constitution VI). The editor therefore remembers the note's
  text, category and favorite when it opens it; if the stored note differs by the time of the save, the text is kept as a
  conflict copy instead of overwriting (complexity item C-2). The scheduler additionally defers while
  an editor page is open, so the window shrinks to a sync that was already running.
- **QML is two small edits**: the switch on the Settings page and an "editor open" notification from
  the editor page. The existing status line, "Syncing..." button state and conflict dialog already
  react to a sync that was started by anyone.

Research resolved every unknown (R-1 to R-8). No device spike is needed: the feature adds no
rendering and no input; the device walkthrough covers timing and Wi-Fi behavior.

## Technical Context

**Language/Version**: C++17 (core, bridge, app); QML (Qt Quick 6.8) — unchanged.

**Primary Dependencies**: unchanged. No new library.

**Storage**: one new row in the existing `settings` table (`sync_after_save`), default `on`,
validated by the store like the other settings. No schema version change, no migration. An upgrade
simply reads the default.

**Testing**: the existing harness — host unit tests (CTest, one executable per `tests/unit/test_*.cpp`),
offscreen Qt bridge tests (`tests/bridge`, real local HTTPS server in `test_notes_bridge`), QML page
tests with injected mouse events (`tests/qml`, stubs in `stubs.qml`), app `--self-test`, manual device
walkthrough. New: `test_sync_scheduler` (unit), `test_settings` additions, `test_notes_bridge`
additions (auto sync with short injected timings), `settings_sync.qml`,
`editor_flow.qml` addition (the editor reports open / closed); `test_store_edit` additions (guarded save, conflict copy).

**Target Platform**: PocketBook InkPad One (Qt 6.8.x runtime, SDK build) and Linux x86-64 host — unchanged.

**Project Type**: single application with a host-testable core library — unchanged.

**Performance Goals**: the save itself is not slower with the switch on (SC-004): the scheduler call
is O(1) arithmetic and the sync runs on the existing worker thread. A saved change reaches the server
within 30 s in 95% of tries on a normal home network (SC-001): quiet period 4 s + at most 15 s gap
after a previous sync start + one sync.

**Constraints**: no note text in logs (event names and counts only, FR-019); TLS verification is
unchanged and always on; no timer-based or background sync (FR-020): the only timer is the
single-shot wait inside the quiet period / gap and it is armed only by a save; e-ink: no UI change
while it runs other than the existing status text and button label.

**Scale/Scope**: 1 new core module (`sync_scheduler`, ≈80 lines) + 1 setting + 1 small store guard;
`NotesBridge` gains 1 property, 1 setter, ≈60 lines of wiring; 2 QML edits; about 1 new test file and
5 existing ones extended.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate (constitution) | Pre-research | Post-design | Evidence |
|---------------------|--------------|-------------|----------|
| I. Test-First | PASS | PASS | The scheduler's rules (quiet period, gap, follow-up, offline drop, editor deferral, off/no-account) are pure functions of numbers and are covered by `test_sync_scheduler` before any wiring exists. Bridge wiring is covered with injected short timings and an injected Wi-Fi callback. `tasks.md` must order each failing test before its code |
| II. Simplicity (YAGNI) | PASS with justified items | PASS with justified items | Two items in Complexity Tracking. Rejected: a delay setting, per-note or per-category rules, retry timers, "sync when Wi-Fi returns", a progress UI, a Settings toast. One on/off switch only |
| III. Security & Privacy | PASS | PASS | Data goes only to the configured server over the unchanged verified connection. No new endpoint, file or log field; log lines carry event names and counts only. Default off, so upgrading starts no traffic |
| IV. Integration tests at device boundaries | PASS | PASS | No boundary format changes. An integration test covers "save → automatic sync → server has the text" against the local HTTPS test server, and "text replaced by a pull before the save → conflict copy" in `test_notes_bridge` |
| V. Host-testable core | PASS | PASS | The scheduler is in `notes_core` without Qt; the bridge supplies the clock, the Wi-Fi answer and the timer |
| VI. No silent data loss | PASS | PASS | An automatic sync is the same engine with the same If-Match / conflict rules. The one new way to lose a version (editing a note a background pull just replaced) is closed by the editor guard (C-2), tested first |
| Stack: C++17, Qt 6.8 Quick/QML, CMake, libcurl, SQLite | PASS | PASS | No stack change |
| SDK location only from `POCKETBOOK_SDK` / `-DSDK_PATH` | PASS | PASS | No build path added (one new core source in the existing list) |
| Both device and host build targets | PASS | PASS | All new code is in existing targets |
| Licensing: InkFeed attributed; notes-android reference-only | PASS | PASS | Nothing copied; behavior written from the spec |
| Release version | PASS | PASS | A MINOR bump to 1.4.0 with a CHANGELOG entry is the last task of the feature |

No violations block planning. Feature 001 FR-031 ("sync only on request or at app start") is refined
by this feature's FR-020 (a completed save is a third, opt-in trigger); the old text gets a pointer
in the feature 001 spec as a task, not a rewrite.

## Project Structure

### Documentation (this feature)

```text
specs/008-autosync-on-save/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R-1 .. R-8
├── data-model.md        # Phase 1: the setting, scheduler state and transitions
├── quickstart.md        # Phase 1: host tests and device walkthrough
├── contracts/
│   ├── core-library.md  # sync_scheduler.h, the setting, the guarded save and the conflict copy
│   └── qml-bridge.md    # NotesBridge additions, Settings control, editor notification
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks — not created here)
```

### Source Code (repository root)

```text
core/include/pbnotes/
├── sync_scheduler.h     # new: SyncScheduler (note_saved, sync_started, next)
└── note_store.h         # + save_local_if_unchanged, save_edit_as_conflict_copy (editor guard); settings doc: sync_after_save
core/src/
├── sync_scheduler.cpp   # new
└── note_store.cpp       # sync_after_save in defaults/validation; guarded save
core/CMakeLists.txt      # + src/sync_scheduler.cpp
bridge/
├── include/notes_bridge.h  # + syncAfterSave property, setEditorOpen(), Config::{wifiConnected, autoSync timings}
└── src/notes_bridge.cpp    # scheduler wiring in the six edit methods and syncNow; guarded saveNote
app/main.cpp             # Config::wifiConnected = device.wifiConnected
qml/pages/
├── SettingsPage.qml     # "Auto Sync" control (new "Synchronization" section)
└── EditorPage.qml       # bridge.setEditorOpen(true/false)
tests/
├── unit/                # test_sync_scheduler (new); test_store_open / test_account_store / test_store_edit (+ setting, guarded save, conflict copy)
├── bridge/              # test_notes_bridge (+ automatic sync, editor guard, setting)
├── integration/         # (no change: the end-to-end cases live in test_notes_bridge with a real local server)
└── qml/                 # settings_sync.qml (new), stubs.qml (+ new bridge members)
VERSION, CHANGELOG.md    # 1.4.0 (last task)
```

**Structure Decision**: the repository's existing layout is kept; no new module or library. The
rules that decide *whether and when* to sync are a core class with host tests; the bridge owns the
clock, the timer, the Wi-Fi callback and the call to the existing `syncNow()`; QML only shows the
switch and reports that an editor is open.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| C-1. New core class `SyncScheduler` (≈80 lines) | FR-006..FR-016 and SC-006 have precise timing rules (quiet period, gap, one follow-up, drop when offline / nothing pending / off, deferral while editing) that must be provable without a device, a network or sleeping | Timer logic written directly in the bridge or QML can only be tested with real delays and a Qt event loop, making tests slow and flaky (constitution I, V) |
| C-2. Guarded save in the editor path | A background sync now overlaps with the reader's use of the app; without the guard a save of stale editor text overwrites a just-pulled server version without a conflict (constitution VI, non-negotiable) | Blocking all editing during a sync would make the app feel frozen on slow networks and contradicts FR-012; ignoring the window leaves a silent-overwrite path that did not exist in practice before this feature |
