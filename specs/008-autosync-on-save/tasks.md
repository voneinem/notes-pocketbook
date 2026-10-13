---

description: "Task list for Autosync on Save"
---

# Tasks: Autosync on Save

**Input**: Design documents from `/specs/008-autosync-on-save/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: REQUIRED. The constitution makes Test-First non-negotiable (Principle I): every test task
below MUST be written and observed FAILING before the implementation task(s) it names (a test that
does not compile against the missing header or signature counts as failing). Tests for `notes_core`
run on the host without Qt, the SDK or a device (Principle V). Unit tests under `tests/unit/` are
picked up by the glob in `tests/CMakeLists.txt`; bridge tests live in `tests/bridge/` and are listed
by name in the bridge loop of `tests/CMakeLists.txt` (this feature adds none: `test_notes_bridge` and
`test_edit_bridge` already exist); the QML page test needs an `add_test(... test_qml_pages <driver.qml>
<page.qml>)` entry in the QML block of `tests/CMakeLists.txt`.

**Organization**: Tasks are grouped by user story. Paths are relative to the repository root. This
feature builds on features 001–006; all existing tests must stay green.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US3 as in spec.md
- **[MANUAL]**: needs a person or the reader; record the result in the named file. An implementing
  agent skips these and reports them as open

## Conventions that apply to every task

- C++17; namespace `pbnotes` in the core; `notes_core` MUST NOT include Qt or `inkview.h` headers
  (`scripts/check-constitution.sh` R3). Qt types appear only in `bridge/`, `app/` and `qml/`.
- **No text in logs**: no log line, `qDebug`/`qWarning`, file or server field may carry note text,
  titles or categories. Event names and counts only. Log keys must not be, start or end with `url`,
  `path`, `file` or `name` (R6): use `reason` and `pending`.
- **Numbers from the design** (apply in code, not by convention): quiet period `4000` ms; gap between
  sync starts `15000` ms; setting `sync_after_save` with values `on` / `off`, default `on`; scheduler
  check order "armed → enabled → configured → running → editor_open → has_pending → waits → online";
  the wait is "the longer of the two remainders" and is never ≤ 0 (then the result is `Start`).
- An automatic sync MUST call the existing `NotesBridge::syncNow()`; it MUST NOT call
  `DeviceBridge::connectNetwork()` and MUST NOT retry on a timer or when Wi-Fi returns (FR-013, FR-016,
  FR-020).
- UI strings go through `qsTr`. Interfaces are those of `contracts/core-library.md` and
  `contracts/qml-bridge.md`; if a task finds a contract wrong, fix the contract in the same commit.
- A new core source file goes into `PBNOTES_CORE_SOURCES` in `core/CMakeLists.txt`. No new `Q_OBJECT`
  header is added (no `pbnotes_moc` change) and no new QML file (no `qml/qml.qrc` change).

---

## Phase 1: Setup

- [X] T001 Build the host target and run the whole suite on the untouched branch
  (`cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-host && ctest
  --test-dir build-host --output-on-failure`). Every test must pass (including `app_self_test` and the
  `qml_*` tests); if one does not, stop and report it — later failures must be attributable to this
  feature.

---

## Phase 2: Foundational (the setting and the scheduler — used by all three stories)

**Purpose**: the persisted preference and the pure decision logic. No bridge or UI behavior changes.

- [X] T002 [P] Write the failing tests in `tests/unit/test_store_open.cpp` (next to `default_settings`):
  a fresh store returns `off` for `get_setting("sync_after_save")`; `set_setting("sync_after_save",
  "on")` and `"off"` return true and are read back, also after closing and reopening the database;
  `"yes"`, `""`, `"ON"` and `"1"` return false and leave the value unchanged; `forget_account()` (with
  the setting `on`) leaves it `on`. Also in `tests/unit/test_store_migration_v3.cpp` or the newest
  migration test: a database created by the previous version reads `off`.
- [X] T003 Add `sync_after_save` to `setting_defaults()` (`"off"`) and `valid_setting()` (`on` or `off`,
  nothing else) in `core/src/note_store.cpp`, and to the documented keys of `get_setting` in
  `core/include/pbnotes/note_store.h`. No schema change and no version bump. T002 passes.
- [X] T004 [P] Write the failing test `tests/unit/test_sync_scheduler.cpp` (header
  `pbnotes/sync_scheduler.h` does not exist yet), one `PB_TEST` per row of the behavior table in
  `contracts/core-library.md`: never saved → `Idle`; `note_saved(0)` then `next(0, good)` →
  `Wait(4000)`; `next(4000, good)` → `Start`; two saves 2 s apart measure the wait from the second;
  `sync_started(t)` with `t >= last_save` disarms; `sync_started(t)` with `t < last_save` leaves it
  armed; a recent `sync_started` makes `next` after the quiet period return `Wait(gap remainder)` and
  the result is the **longer** of the two remainders; `enabled=false` → `Idle` reason `"disabled"` and
  disarmed; `configured=false` → `"no_account"` and disarmed; `running=true` → `Idle` `"running"` and
  still armed; `editor_open=true` → `Idle` `"editor_open"` and still armed; `has_pending=false` →
  `Idle` `"nothing_pending"` and disarmed; `online=false` once the waits are over → `Idle` `"offline"`
  and disarmed (a later `next` stays `Idle`: no retry); the order of the checks (e.g. `enabled=false`
  and `running=true` → `"disabled"`); the quiet and gap values come from `Options`; and the
  ten-saves scenario (saves 6 s apart over 60 s, `next` called at every 1 s step, a `Start` is followed
  by `sync_started`) produces at most 5 `Start`s with default options. Build must fail on the missing
  header; after T005 it must pass.
- [X] T005 Create `core/include/pbnotes/sync_scheduler.h` and `core/src/sync_scheduler.cpp` exactly as
  in `contracts/core-library.md` (`Options`, `Inputs`, `Decision`, `Step`, `note_saved`,
  `sync_started`, `next`, `armed`; time is a parameter, no clock, no thread, no Qt). Add
  `src/sync_scheduler.cpp` to `PBNOTES_CORE_SOURCES` in `core/CMakeLists.txt`. T004 passes.

**Checkpoint**: `ctest` green; nothing in the app behaves differently yet.

---

## Phase 3: User Story 2 — The reader chooses whether it happens (Priority: P1)

**Goal**: Settings has "Auto Sync", on by default, effective at once, remembered across
restarts, usable without an account.

**Independent Test**: Open Settings: the button reads "Auto Sync: on". Switch it off, restart
the app: still on. (Without the wiring of US1 the switch changes only the stored value.)

- [X] T006 [P] [US2] Write the failing bridge tests in `tests/bridge/test_edit_bridge.cpp` (or next to
  `the_rotation_preference_is_validated_and_persisted` in `tests/bridge/test_notes_bridge.cpp`):
  `syncAfterSave()` is false on a fresh bridge; setting it true emits `syncAfterSaveChanged` once and
  persists across a new `NotesBridge` on the same data folder; setting the same value again emits
  nothing; it works with no account saved; `forgetAccount()` keeps it.
- [X] T007 [US2] In `bridge/include/notes_bridge.h` add `Q_PROPERTY(bool syncAfterSave READ
  syncAfterSave WRITE setSyncAfterSave NOTIFY syncAfterSaveChanged)`, the getter/setter and the signal;
  in `bridge/src/notes_bridge.cpp` back them with `get_setting`/`set_setting("sync_after_save",
  "on"|"off")` like `rotationPreference`. T006 passes.
- [X] T008 [P] [US2] Write the failing QML page test `tests/qml/settings_sync.qml` and register it as
  `qml_settings_sync` in the QML block of `tests/CMakeLists.txt` (page
  `qml/pages/SettingsPage.qml`, same ENVIRONMENT/LABELS as the other `qml_*` tests). Extend the
  `bridge` object in `tests/qml/stubs.qml` with a writable `property bool syncAfterSave: false` and
  `function setEditorOpen(open) { editorOpenCalls.push(open) }` (a `property var editorOpenCalls: []`).
  The test asserts: the button text is `Auto Sync: off`; a click sets
  `bridge.syncAfterSave` to true and the text becomes `Auto Sync: on`; a second click flips it
  back; the explanation text is shown. (The editor assertions of the contract are added in T022.)
- [X] T009 [US2] In `qml/pages/SettingsPage.qml` add a "Synchronization" section (heading in the style
  of the others, 36 px bold) between "Screen rotation" and "Account": an `InkButton` with text
  `qsTr("Auto Sync: on")` / `qsTr("Auto Sync: off")` that sets `bridge.syncAfterSave =
  !bridge.syncAfterSave`, and under it a 28 px wrapped text: "When on, the reader syncs a few seconds
  after you save a change, if Wi-Fi is connected. When off, notes sync only with Sync now and at
  start." Update the page's header comment. T008 passes; `app_self_test` still passes.

**Checkpoint**: the switch exists, persists and does nothing else yet.

---

## Phase 4: User Story 1 — A saved note reaches the server without a second step (Priority: P1) 🎯 MVP

**Goal**: with the switch on and Wi-Fi connected, a save, delete or other saved change starts the
existing sync by itself, without blocking the save.

**Independent Test**: set `sync_after_save` to `on`, with the local test server up save an edited
note: the server receives the new text without `syncNow()` being called by the test, and the sync
status shows a successful outcome.

- [X] T010 [P] [US1] Write the failing tests in `tests/bridge/test_notes_bridge.cpp`, using the local
  HTTPS server and `Config` of `syncing_pulls_the_notes_and_reports_the_outcome`, with new
  `Config::autoSyncQuietMs = 50`, `autoSyncGapMs = 0` and `wifiConnected = []{ return true; }` (these
  fields do not exist yet, so the test does not compile): (a) switch on, `saveNote` of an edited
  synced note, pump the Qt event loop until `syncFinished` (timeout 5 s) → the server received exactly
  one write for it and `syncOutcome()` is `success`; (b) same for a new note (created on the server);
  (c) `deleteNote` of a synced note → the server received the delete; (d) `toggleFavorite`,
  `setCategory` and `toggleTask` each start a sync that sends the change; (e) switch **off**: after the
  same saves and 500 ms of event-loop time the server received **no** request at all and
  `syncRunning()` was never true (SC-003); (f) `saveNote` returns before any request is made (the
  server counter is still 0 immediately after the call) (FR-012); (g) the save of an unchanged note
  (nothing pending) starts no sync; (h) a call to `syncNow()` made by the test right after a save
  (inside the quiet period) leaves no second sync behind (`sync_started` disarmed it): exactly one
  sync ran.
- [X] T011 [US1] In `bridge/include/notes_bridge.h` / `bridge/src/notes_bridge.cpp` add to
  `Config`: `std::function<bool()> wifiConnected` (empty = online), `int autoSyncQuietMs = 4000`,
  `int autoSyncGapMs = 15000`. Add members `pbnotes::SyncScheduler scheduler_`, a single-shot
  `QTimer* autoTimer_` (parent `this`) and a monotonic clock helper (`std::chrono::steady_clock` in
  milliseconds). Add private `void noteSaved()` = `scheduler_.note_saved(now)` + `evaluateAutoSync()`,
  and `void evaluateAutoSync()`: builds `SyncScheduler::Inputs` (`enabled` from the setting,
  `configured` from `store_->get_account()`, `running` from `syncRunning_`, `editor_open` = false for
  now, `has_pending` = `!store_->pending_notes().empty()`, `online` from the callback), calls
  `scheduler_.next(now, in)`, and on `Start` calls `syncNow()`, on `Wait` restarts `autoTimer_` for
  `wait_ms` (connected to `evaluateAutoSync`), on `Idle` stops the timer. Log
  `autosync.armed` / `autosync.start pending=<n>` / `autosync.drop reason=<code>` (codes and counts
  only).
- [X] T012 [US1] Call `noteSaved()` at the end of the success paths of `saveNote` (new and existing),
  `deleteNote`, `setCategory`, `toggleFavorite` and `toggleTask` in `bridge/src/notes_bridge.cpp`
  (after the `noteChanged` / `notesChanged` emits; for `toggleTask` only on the path that really
  changed the content; never on a failed save). In `syncNow()` call `scheduler_.sync_started(now)` once
  the guard (`!store_ || no account || syncRunning_.exchange(true)`) has passed. T010 (a)–(h) pass.
- [X] T013 [US1] In `app/main.cpp` set `config.wifiConnected = [&device]() { return
  device.wifiConnected(); }`. `DeviceBridge` is created after `NotesBridge` and reads
  `notes.rotationPreference()`, so do not reorder them: capture a `DeviceBridge* devicePtr = nullptr;`
  in the lambda (`return devicePtr && devicePtr->wifiConnected();`) and assign `devicePtr = &device;`
  right after `DeviceBridge device;`; keep the existing rotation wiring working. Do not call `connectNetwork()` anywhere in the
  new path. `app_self_test` still passes.
- [X] T014 [P] [US1] Write the failing integration test in `tests/integration/test_edge_cases.cpp`
  (or `tests/bridge/test_notes_bridge.cpp` if the HTTPS server is only available there): after an
  automatic sync that follows a save, a note holding the markdown of feature 006 (fence, quote,
  details) has identical text on the server (FR-009: the automatic sync is the same engine), and a
  server-side change to the same note made before the save is handled as a manual sync handles it —
  both versions exist and `conflictsDetected(1, 0)` was emitted (US1 scenario 6, FR-017, SC-007).
  If T012 already makes it pass, record that in the test comment (characterization, written first).

**Checkpoint**: MVP. Switch on → saves sync on their own. Not yet: offline/busy polish (US3).

---

## Phase 5: User Story 3 — Offline and busy situations stay quiet and safe (Priority: P2)

**Goal**: no error or delay without Wi-Fi; no overlapping syncs; one follow-up for a save made during
a sync; saves close together combined; no sync under an open editor; a stale editor text never
overwrites a just-pulled version.

**Independent Test**: switch on, Wi-Fi off, save three notes: saved, no error, shown as waiting; Wi-Fi
on, save a fourth: all four reach the server.

- [X] T015 [P] [US3] Write the failing bridge tests in `tests/bridge/test_notes_bridge.cpp` (same
  fixtures as T010): (a) `wifiConnected` returns false: after a save and 500 ms of event-loop time the
  server saw no request, `errorOccurred` was not emitted, `syncFinished` was not emitted, and
  `pendingCount()` is 1; the callback is flipped to true and a second save syncs **both** notes
  (US3-2); (b) a server that answers 503 / refuses the connection: the automatic sync ends with a
  failed/offline outcome in `syncStatus()`, no dialog signal (`conflictsDetected`) and **no second
  sync** within 1 s although the quiet period and gap are over (FR-016, no retry loop); (c) a save
  while a sync is running (hold the server's response until after the save): after the first sync ends
  exactly one more sync runs and the second change reaches the server (US3-4, FR-014); (d) with
  `autoSyncQuietMs = 200`, five saves made within 100 ms cause exactly one sync that carries all five
  (US3-5, FR-015); (e) the switch set to off while a sync is running: that sync finishes normally and
  no further sync starts (US2-4); (f) with the switch on and **no account** saved, a save
  starts no sync and emits no `errorOccurred` (FR-005);
  (g) `setEditorOpen(true)` then a save: no sync for 500 ms; `setEditorOpen(false)` → the sync starts.
- [X] T016 [US3] In `bridge/src/notes_bridge.cpp` make T015 (a)–(f) pass: in the `syncNow()` completion
  lambda call `evaluateAutoSync()` after the `syncFinished` / `conflictsDetected` emits (the single
  follow-up); make `setSyncAfterSave` call `evaluateAutoSync()` (off disarms through `enabled=false`;
  on starts nothing by itself because the scheduler is not armed). Ensure a failed sync arms nothing.
- [X] T017 [US3] Add `Q_INVOKABLE void setEditorOpen(bool open)` to `NotesBridge` (member
  `editorOpen_`; the `editor_open` input of `evaluateAutoSync()` reads it; closing calls
  `evaluateAutoSync()`; log `autosync.deferred reason=editor_open` when it is the reason). T015 (g)
  passes.
- [X] T018 [P] [US3] Write the failing store tests in `tests/unit/test_store_edit.cpp`:
  `save_local_if_unchanged(local_id, base, ...)` (with `EditBase{content, category, favorite}`) writes and
  returns no error when the stored text, category and favorite equal `base` - also when a pull changed only
  the ETag (the reader's own push, confirmed meanwhile); returns `EditError::ChangedOnServer` with
  `error_key "changed_on_server"` and writes **nothing** (content, status, ETag, modified unchanged) when a
  pull replaced the text, or changed only the category or the favorite mark; a local-only note and an unknown
  id behave as `save_local` (`None` / `NotFound`). And `save_edit_as_conflict_copy(original_id, title,
  content, category, favorite, now)` inserts a new pending note titled `title + conflict_suffix(now, 1)`
  (then number 2, 3 ... when taken), with the given text, category and favorite; the original is untouched;
  invalid input (blank text, bad category, empty title) is refused and stores nothing.
- [X] T019 [US3] In `core/include/pbnotes/note_store.h` / `core/src/note_store.cpp` add `EditError::ChangedOnServer`,
  `struct EditBase`, `save_local_if_unchanged` and `save_edit_as_conflict_copy` as specified in
  `contracts/core-library.md`. Move the unused-title loop of `resolve_conflict` into one private helper
  (`unused_conflict_title_locked`) that both call, so the naming rule exists once; `test_conflict_title` and
  `test_conflict_atomic` stay green unchanged. Staged attachments the copy's text names are bound to it and
  downloaded ones of the original are duplicated (`copy_attachments_for_conflict_locked`). T018 passes.
- [X] T020 [P] [US3] Write the failing bridge tests in `tests/bridge/test_notes_bridge.cpp` (the file that already
  holds the editor tests of spec 004): `loadForEdit(id)`, then change the stored note the way a pull does
  (`store()->apply_remote({...})` with a newer ETag and text), then `saveNote(id, ...)` with the old text -> the
  stored note keeps the pulled text, a **new** note exists whose title carries ` (conflict ` and whose text is the
  editor's text, the returned id is the new note's, and `conflictsDetected(1, 0)` was emitted once (FR-017);
  without an intervening pull `saveNote` behaves as before (no copy, no signal); a pull that changed only the
  ETag is no conflict; a local-only note and a new note are saved normally; loading another note does not disturb
  the first one's base; a save without a prior `loadForEdit`, a refused (blank) save followed by a retry, and a
  save after a successful save are unguarded / keep the base as described.
- [X] T021 [US3] In `bridge/include/notes_bridge.h` / `bridge/src/notes_bridge.cpp`: make `loadForEdit` remember an
  `EditBase` (text, category, favorite) in a `mutable std::map<qlonglong, pbnotes::EditBase> editBase_`; change the
  existing-note branch of `saveNote` to call `save_local_if_unchanged` when a base is remembered, and on
  `ChangedOnServer` call `save_edit_as_conflict_copy`, emit `noteChanged` for both notes, `notesChanged`,
  `conflictsDetected(1, 0)`, log `note.conflict_on_save` (a count only), and return the copy's id; erase the base
  after a successful save, in `deleteNote`, and (all of them) when `setEditorOpen(false)` is called. T020 passes;
  the earlier editor tests stay green.
- [X] T022 [US3] In `qml/pages/EditorPage.qml` add `Component.onCompleted` handling (merge with the
  existing handler, if any) calling `bridge.setEditorOpen(true)` and `Component.onDestruction:
  bridge.setEditorOpen(false)`; update the page's header comment. Extend `tests/qml/editor_flow.qml`
  (stub `editorOpenCalls` from T008): opening the page records `true`, destroying it records `false`.
  `qml_editor_flow` and `qml_editor_slash` pass.
- [X] T023 [P] [US3] The offline rows of `quickstart.md` §2 ("offline callback", "failed automatic sync", "editor open")
  are all covered by the T015 cases; `quickstart.md` now names the real test files and cases.

**Checkpoint**: all three user stories work.

---

## Phase 6: Polish & cross-cutting concerns

- [X] T024 [P] Record the refinement in the feature 001 spec: in
  `specs/001-pocketbook-notes-mvp/spec.md` FR-031 add one sentence "Refined by feature 008: with the
  setting 'Auto Sync' on, a completed save is a third trigger." Do not rewrite the requirement.
- [X] T025 Review: run `scripts/check-constitution.sh`; grep the diff for `qDebug`, `qWarning` and
  `log::` calls and confirm none carries note text, titles, categories or paths (codes and counts
  only); confirm `connectNetwork` is not referenced by any new code; confirm the TLS code and
  `CurlTransport` are untouched; confirm no new dependency, no schema change and no new file format;
  confirm `sync_after_save` is not touched by `forget_account`.
- [X] T026 [P] Update `README.md` (feature list: "Auto Sync" setting, on by default, what it
  does and does not do: no timer, no Wi-Fi switch-on, one sync per burst of saves) and the Settings
  paragraph of `INSTALL.md` if it describes the Settings page.
- [X] T027 Release: raise `VERSION` from `1.3.1` to `1.4.0` (MINOR: a user-visible feature, nothing
  breaking), add the `## 1.4.0` entry to `CHANGELOG.md` (Settings: "Auto Sync", on by default;
  a saved change starts a sync a few seconds later when Wi-Fi is connected; nothing happens offline;
  changes close together share one sync; a text saved from an editor that was open while a sync
  replaced the note is kept as a conflict copy; no change to the server, the data or the sync
  protocol), rebuild, and run `scripts/check-version-bump.sh` as `scripts/pre-merge.sh` does. Tag
  `v1.4.0` after the merge.
- [X] T028 Run `scripts/pre-merge.sh` (all tests, constitution checks, version bump) on the final tree
  and fix what it reports.
- [ ] T029 [MANUAL] Walk `quickstart.md` §3 (all 13 rows) on the reader with the release package.
  Record in `specs/008-autosync-on-save/checklists/device-results.md`; measure rows 3 and 7 against
  SC-001 (30 s) and row 8 against SC-006, and note the measured times.
- [X] T030 Update `specs/008-autosync-on-save/checklists/requirements.md` notes: every FR-001..FR-020
  is covered by a task above (map: FR-001, FR-002, FR-004 → T002, T003, T006, T007, T009; FR-003 →
  T006, T016; FR-005 → T010, T015(f); FR-006..FR-008 → T010–T012; FR-009 → T014; FR-010 → T010(e);
  FR-011 → T015(g), T017; FR-012 → T010(f); FR-013 → T015(a); FR-014 → T015(c); FR-015 → T004, T015(d);
  FR-016 → T015(b); FR-017 → T014, T018–T021; FR-018 → T010, T029; FR-019 → T011, T025; FR-020 →
  T011, T024, T025); list the open [MANUAL] tasks.

---

## Dependencies & Execution Order

- T001 first. Phase 2: T002 → T003 and T004 → T005 are two independent chains (different files), so
  T002 ‖ T004 may be written together; each implementation follows its test.
- **US2** (Phase 3) needs T003. T006 → T007; T008 → T009 (T008 edits `stubs.qml`, which US3's T022
  also uses; do T008 first).
- **US1** (Phase 4) needs T005 (scheduler) and T003 (setting; the bridge reads it through the store, so
  US1 does not need T007). T010 ‖ T014 are independent files/cases; T011 needs T010 (to compile the
  test against the missing `Config` fields) → T012 → T013 (after T011, same `Config`).
- **US3** (Phase 5) needs US1's T011/T012 for the wiring. T015 → T016 → T017 are sequential (same
  files). The guard chain T018 → T019 (core) and T020 → T021 (bridge, needs T019) is independent of
  T015–T017 until both edit `notes_bridge.cpp` (do T021 after T017). T022 needs T017 and T008.
- Polish needs the stories it documents or runs. T027 (version bump) comes last before the merge;
  T028 after it; T029 on the release package.

### Parallel opportunities

- Phase 2: T002 ‖ T004 (tests), then T003 ‖ T005.
- After Phase 2: US2 (T006–T009, settings/QML) ‖ US1 (T010–T014, bridge core wiring) by two people:
  they share only `notes_bridge.h/.cpp` (T007 and T011) — do T007 first, it is a few lines.
- US3: T015 ‖ T018 ‖ T020 (tests in different files); T019 ‖ T016/T017 (core vs bridge).
- Polish: T024 ‖ T026.

## Implementation Strategy

- **MVP = Phase 1 + Phase 2 + US2 + US1** (T001–T014): the switch exists and saves sync on their own
  with the default timings. Ship-ready only together with US3's T016/T017 for the follow-up and the
  editor deferral, and T018–T021 for the overwrite guard — the guard is a data-safety requirement
  (constitution VI) once a sync can overlap an editor, so **do not release US1 without T021**.
- Then US3's offline/busy cases (most already follow from the scheduler tested in T004; T015–T017
  prove the wiring). Last the polish, release bump and the device walkthrough.
- Each phase ends with the whole suite green. Commit per task or per coherent group, test first.
