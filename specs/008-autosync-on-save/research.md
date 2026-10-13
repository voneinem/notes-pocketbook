# Research: Autosync on Save

Resolves what the plan's Technical Context left open. Facts were read from this repository (core,
bridge, QML, tests, feature 001 spec); nothing was probed on the device.

## R-1. Where an automatic sync is started (FR-006 – FR-012)

- **Findings**: Every local change goes through `NotesBridge`: `saveNote`, `deleteNote`,
  `setCategory`, `toggleFavorite`, `toggleTask` (`bridge/src/notes_bridge.cpp`), each emitting
  `noteChanged` + `notesChanged` after the store call succeeded. Attachments added in the editor are
  staged and bound by `saveNote`, so they are covered by it. `syncNow()` is the single entry to the
  sync: it returns at once without an account or while one runs, and runs the engine on a worker
  thread. The list page starts it today ("Sync now", and once at start if Wi-Fi is up) after its own
  Wi-Fi check (`NoteListPage.qml`).
- **Decision**: the bridge starts the automatic sync, not a QML page. A save can happen on three pages
  (editor, reader checkbox, list) and the editor closes at once; a page-level hook would miss or
  duplicate. The six methods call one private `noteSaved()`, which asks the scheduler (R-2) and calls
  the existing `syncNow()`.
- **Alternatives considered**: a QML `Connections` on `noteChanged` in `Main.qml` (works, but moves
  the Wi-Fi check and the timing rules into untestable QML and needs the bridge object anyway); a hook
  inside `NoteStore` (the store is the layer under the sync engine and must not start it).

## R-2. Scheduler rules and numbers (FR-013 – FR-016, SC-001, SC-006)

- **Findings**: one sync reads the pending list when it starts; a save made after that is not part of
  it. A sync takes seconds (HTTPS round trips per changed note). The spec asks for: combine close
  saves; never overlap; one follow-up when saved during a run; a failed or offline attempt is not
  retried; ten saves in a minute give at most five syncs; a saved change reaches the server within 30 s
  in 95% of tries.
- **Decision**: `SyncScheduler` (Qt-free, time passed in as milliseconds):
  - `note_saved(now)` arms it and records the time.
  - `sync_started(now)` is called for **every** sync, manual or automatic: it records the start time
    (for the gap) and disarms if the sync started after the last save (it read that change).
  - `next(now, inputs)` returns `Idle`, `Wait(ms)` or `Start`:
    1. not armed → `Idle`;
    2. switch off or no account → disarm, `Idle`;
    3. a sync is running → `Idle` (stays armed; the bridge asks again when that sync ends — this is
       the single follow-up);
    4. an editor page is open → `Idle` (stays armed; the bridge asks again when the editor closes);
    5. nothing pending any more → disarm, `Idle`;
    6. quiet period (4 s since the last save) or gap (15 s since the last sync start) not over →
       `Wait(longer of the two remainders)`;
    7. Wi-Fi not connected → disarm, `Idle` (no retry: the change goes with the next sync, FR-013);
    8. otherwise `Start`.
  The gap exists for SC-006: saves 6 s apart with only a quiet period would still be one sync each.
  Worst-case delay of SC-001: 15 s gap + a sync of a few seconds, inside 30 s.
- **Constants**: quiet 4 s, gap 15 s, both overridable through `NotesBridge::Config` so bridge tests
  run in milliseconds. No setting (YAGNI).
- **Alternatives considered**: debounce only (fails SC-006); a fixed 30 s timer after the first save
  (misses SC-001 and feels broken when the reader is waiting); sync on every save with no combining
  (floods the server and the status line, violates FR-015); retry when Wi-Fi returns (background sync,
  out of scope per FR-020).

## R-3. Wi-Fi check and no Wi-Fi switch-on (FR-013, edge case "Wi-Fi on but not connected")

- **Findings**: `DeviceBridge::wifiConnected()` is `QueryNetwork() & NET_CONNECTED`; `connectNetwork()`
  asks the system to connect and can show a system dialog. The start-up sync and the attachment worker
  use the callback pattern (`AttachmentsBridge::Config::wifiConnected`, empty = always online).
- **Decision**: `NotesBridge::Config::wifiConnected` (same type and meaning), set in `app/main.cpp`.
  **Revised after the first device test (2026-10-12):** the reader's Wi-Fi is normally off, so "drop when
  offline" meant Auto Sync did nothing (`autosync.drop reason=offline` in the device log). Now the scheduler
  returns `Connect` when offline and the caller can connect (`NotesBridge::setWifiRequest`, set in
  `app/main.cpp` to `DeviceBridge::connectNetwork()`, the request "Sync now" makes); saves made while the
  device connects share that one request. `networkAnswered(true)` asks the scheduler again (the sync starts);
  a failed connection drops the start quietly (one attempt per save, no timer, no retry). Without a
  request callback (tests, no device) offline still drops. A failed sync with Wi-Fi up (server down, bad
  credentials) is reported by the existing status line and is not retried (FR-016).
- **Alternatives considered**: never calling `connectNetwork()` (the first design; rejected after the device test,
  because the feature then does nothing on a reader whose Wi-Fi is off).

## R-4. The preference and where it lives (FR-001 – FR-005)

- **Findings**: `setting_defaults()` / `valid_setting()` in `note_store.cpp` hold the persisted
  settings (`font_size`, `sort_order`, `view`, `rotation`); `forget_account` clears only the account
  row and setup drafts, so settings survive it. Rotation is exposed as a bridge property backed by
  `get_setting`/`set_setting` and a toggle button in `SettingsPage.qml`.
- **Decision**: key `sync_after_save`, values `on`/`off`, default `on` (changed after the first device test); bridge property
  `syncAfterSave` (bool, read/write, `syncAfterSaveChanged` signal), toggled with a button in a new
  "Synchronization" section of Settings, labelled "Auto Sync: on" / "off". Setting it to off
  disarms the scheduler (a running sync finishes, scenario 2.4); setting it to on does not start a
  sync. Unknown or invalid values are refused by the store, as today.
- **Alternatives considered**: a checkbox control (the app has no switch component; the rotation
  button is the established pattern for a persisted choice on this page); a separate preferences file
  (a second storage mechanism for one boolean).

## R-5. Status, progress and errors reuse (FR-009, FR-016, FR-018)

- **Findings**: `syncNow()` already sets `syncRunning`, emits `syncStateChanged`, `notesChanged`,
  `syncFinished` and `conflictsDetected`; the list page shows the status line and a "Syncing..." button
  from those properties. Failures are not dialogs; only conflicts open the one-time dialog.
- **Decision**: nothing new. A sync started by the scheduler is the same call, so status, outcome and
  the conflict dialog behave as for a manual one. The conflict dialog lives in `Main.qml`, so it shows
  on whichever page the reader is on.
- **Note**: the dialog appearing while the reader types in another note is the one disturbance a
  background sync can cause. It is shown once per sync that found a conflict, as specified (scenario
  1.6), and does not take away editor text.

## R-6. A background pull can replace the text open in the editor (constitution VI)

- **Findings**: `update_locked` (the store's edit path) reads the **current** stored note and writes
  the new text, marking it edited; the engine pushes with the stored ETag. The pull replaces the text
  of notes without local changes with the server's newer version and updates the stored ETag. Today
  the editor can overlap with a sync only for the start-up sync (rare). With automatic sync, a sync
  starts a few seconds after a save, exactly when the reader may open the next note.
  Sequence that loses a version: editor loads note B (ETag e1) → pull stores B' (ETag e2) → reader saves
  the old text → stored as pending with ETag e2 → push uses `If-Match: e2` → accepted → the server's B'
  is overwritten with no conflict.
- **Decision**: two layers.
  1. Defer: the scheduler does not start while an editor page is open (`setEditorOpen`), so no sync
     *starts* under an open editor.
  2. Guard: `NotesBridge::loadForEdit` remembers what the editor loaded (text, category, favorite);
     `saveNote` for an existing note calls a store method `save_local_if_unchanged(local_id, base, ...)`
     that refuses with `EditError::ChangedOnServer` when the stored text, category or favorite differ from
     that base. The bridge then stores the text as a **new note** titled by the store's conflict rule
     (" (conflict YYYY-MM-DD)", then " #2"... for an unused title, shared with `resolve_conflict`), keeps
     the pulled version as the note, and emits `conflictsDetected(1, 0)` so the existing dialog explains it.
  3. Why compare the text and not the ETag: the reader's own push changes the ETag too. Save a note, reopen
     it before that sync ends, and an ETag guard would raise a false conflict although nobody else touched
     the text. A pull that brings a new ETag with the same text is an ordinary save. The title is not
     compared: a confirmed upload may rewrite it (the server's unique title) without any change of the text.
- **Alternatives considered**: refuse the save and keep the editor open (no data lost, but the reader
  must copy the text out by hand); lock the note against the pull (needs a skip list in the engine);
  make the pull defer while an editor is open (engine change for a rare case). The copy keeps both
  versions with the dialog and wording the app already has.

## R-7. Logging and privacy (FR-019)

- **Decision**: log events carry codes and counts only: `autosync.armed`, `autosync.start`
  (`pending=<n>`), `autosync.drop` (`reason=offline|nothing_pending|disabled|no_account`),
  `autosync.deferred` (`reason=editor_open|running`). No title, text or category appears anywhere. The
  existing `NoteStore` redaction rules apply unchanged.

## R-8. Test strategy (constitution I, IV, V)

| Test | Proves | Level |
|------|--------|-------|
| `test_sync_scheduler` | each rule of R-2 with plain numbers: quiet period, gap, combine, follow-up, drop offline / nothing pending / off / no account, editor deferral, `sync_started` disarming, a save after a start re-arming | unit, no Qt |
| `test_store_open` / `test_account_store` / migration test (+) | `sync_after_save` default `on`, accepts `on`/`off`, refuses others, survives `forget_account` and an upgrade | unit |
| `test_store_edit` (+) | `save_local_if_unchanged` accepts an unchanged base (also when only the ETag changed), refuses a replaced text / category / favorite without writing; `save_edit_as_conflict_copy` inserts a numbered copy and leaves the original alone | unit |
| `test_notes_bridge` (+) | with the switch on and the local HTTPS server, a save reaches the server without `syncNow()`; with it off nothing is requested; offline callback → no request, no error signal; a save during a running sync produces exactly one more sync; several close saves produce one | bridge, real server, short injected timings |
| `test_notes_bridge` (+) | `syncAfterSave` persists and emits once; `saveNote` after a pull replaced the text creates the conflict copy and emits `conflictsDetected`; an unchanged note, a local-only note and an unloaded note save as before | bridge |
| `test_notes_bridge` (+, real server) | a server-side change before the save → both versions after the automatic sync; Markdown of feature 006 arrives byte for byte | bridge, integration |
| `settings_sync.qml` | the Settings button shows the state, toggles it, and the stub records the call | QML page test |
| `app --self-test` | still passes with the new QML | existing |
| Device walkthrough | quickstart §3: real Wi-Fi on / off, timing, status line, conflict dialog | manual |

Every test is written first and observed failing before its implementation (constitution I).
