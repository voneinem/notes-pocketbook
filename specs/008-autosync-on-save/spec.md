# Feature Specification: Autosync on Save

**Feature Branch**: `feature/008-autosync-on-save`

**Created**: 2026-10-12

**Status**: Draft

**Input**: User description: "Autosync on Save - configurable via Settings"

## Clarifications

### Session 2026-10-12

- Q: Should every saved change start a sync (Save, delete, favorite, category, task-list checkbox, attachments), or only the editor's Save and delete? → A: Every saved change starts a sync, with saves close together combined (FR-008 and FR-015 stand as written).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A saved note reaches the server without a second step (Priority: P1)

A reader turns on "Auto Sync" in Settings. From then on, when they save a note (a new
note, an edit, or a deletion) and the reader is online, the app starts a sync by itself, so the
change is on the server a few moments later. They no longer have to leave the editor, go back to
the list and press "Sync now" to get the note off the device.

**Why this priority**: This is the whole feature. Today a saved note waits on the device until
the reader remembers to sync or restarts the app, and a device that is lost, reset or run flat
before then takes the unsynced notes with it. Pushing a change as soon as it is saved closes
that gap and makes the server a reliable copy.

**Independent Test**: Turn the setting on, with Wi-Fi connected edit a note and tap Save. Without
touching "Sync now", look at the note on the server (or in another client) within a minute and
verify the new text is there, and that the sync status shows a successful sync with a fresh time.

**Acceptance Scenarios**:

1. **Given** the setting is on and Wi-Fi is connected, **When** the reader saves an edited note,
   **Then** a sync starts without further input, the edit reaches the server, and the status area
   shows the time and a successful outcome of that sync.
2. **Given** the setting is on and Wi-Fi is connected, **When** the reader saves a newly created
   note, **Then** the note is created on the server by the automatic sync.
3. **Given** the setting is on and Wi-Fi is connected, **When** the reader deletes a note and
   confirms, **Then** the deletion is sent to the server by the automatic sync.
4. **Given** the setting is on, **When** the reader changes a note's favorite mark or category, or
   toggles a task-list checkbox on the reading page, **Then** the change is saved and sent by the
   automatic sync like any other saved change.
5. **Given** the setting is on, **When** the reader taps Save, **Then** the editor responds and
   closes at once, exactly as it does with the setting off; it never waits for the network.
6. **Given** the automatic sync finds that the note was also changed on the server, **When** it
   runs, **Then** it is handled exactly as a manual sync handles it: both versions are kept, none
   is overwritten, and the reader is told once, as today.

---

### User Story 2 - The reader chooses whether it happens (Priority: P1)

Settings has a clearly labelled control, "Auto Sync", that switches the behavior on and
off. It shows its current state at a glance, takes effect immediately without a restart, and the
choice is remembered across restarts. It is on on a fresh install and on every device that
upgrades to this version (changed after the first device test: the reader expects a feature called
"Auto Sync" to work without being found in Settings first), and the reader can switch it off.

**Why this priority**: The request says "configurable": a reader who does not want the network
traffic needs one clear switch to turn it off, and it must be honored at once.

**Independent Test**: Open Settings and verify the control exists and reads "on". Switch it off,
leave Settings, force-quit and restart the app, and verify it still reads "off". Save a note
with it off and verify nothing is sent; switch it on and save again and verify it is sent.

**Acceptance Scenarios**:

1. **Given** a fresh install or an upgrade from an earlier version, **When** the reader opens
   Settings, **Then** "Auto Sync" is present and set to on.
2. **Given** the control is off, **When** the reader saves, changes or deletes a note, **Then** no
   sync starts because of it, and the behavior is identical to today (manual sync and sync at app
   start only).
3. **Given** the reader switches the control on, **When** they save the next note, **Then** that
   save already triggers a sync, with no restart and no need to reopen the note.
4. **Given** the control is on, **When** the reader switches it off while a sync is running,
   **Then** the running sync finishes normally and no further automatic sync starts.
5. **Given** the control was set to on, **When** the app is restarted, **Then** it is still on.
6. **Given** no account has been set up, **When** the reader opens Settings, **Then** the control
   is still shown and can be changed, and saving notes does not attempt any sync.

---

### User Story 3 - Offline and busy situations stay quiet and safe (Priority: P2)

Saving must never feel worse with the setting on. Without Wi-Fi, or when the server cannot be
reached, the reader's note is still saved on the device and simply waits, with no error pop-up
and no delay; it goes out on the next sync, manual or automatic. If several saves happen close
together, or a save happens while a sync is already running, the changes are all sent without
the app starting overlapping syncs or hammering the server once per keystroke-sized change.

**Why this priority**: The reader uses this on a train, in a waiting room and on a flaky home
network. The feature earns trust only if the failure cases are as harmless as the happy one.

**Independent Test**: With the setting on and Wi-Fi off, save three notes; verify each is saved,
no error dialog appears, and the status shows the notes as waiting. Turn Wi-Fi on, save a fourth
note, and verify all four reach the server. Then save two notes within a few seconds of each
other and verify both arrive and that only one sync at a time ran.

**Acceptance Scenarios**:

1. **Given** the setting is on and Wi-Fi is not connected, **When** the reader saves a note,
   **Then** the note is saved at once, the reader asks the device to connect (as "Sync now" does)
   and syncs when the connection is up; if the connection fails, no sync is attempted, no error is
   shown, and the note stays marked as waiting to sync.
2. **Given** notes are waiting from offline saves and the setting is on, **When** the reader later
   saves a note with Wi-Fi connected, **Then** the sync that follows also sends the earlier
   waiting notes, since every sync sends all pending changes.
3. **Given** the setting is on and the server cannot be reached or rejects the login, **When** an
   automatic sync fails, **Then** the status area shows what failed exactly as for a manual sync,
   no note is lost or altered, and the reader is not interrupted by a dialog.
4. **Given** a sync is already running (started manually, at app start, or by an earlier save),
   **When** the reader saves another note, **Then** no second overlapping sync starts, and the new
   change is sent by a sync that starts right after the running one ends.
5. **Given** the reader saves several notes in quick succession, **When** the automatic syncs run,
   **Then** the changes are sent together in as few syncs as practical, not one sync per save.
6. **Given** the setting is on and the reader keeps using the app, **When** an automatic sync is
   running in the background of the UI, **Then** the reader can keep browsing, reading and
   editing, and the sync status shows it as in progress.
7. **Given** the reader is in the editor with unsaved changes, **When** they have not tapped Save,
   **Then** no sync is triggered; only a completed save triggers one, and the unsaved text is not
   sent.

---

### Edge Cases

- A note saved with no change in the text (Save tapped without edits): nothing new is pending, so
  no sync needs to be started on its account.
- The app is closed or the reader powers off a few seconds after saving, before the automatic sync
  finishes: the saved change is not lost; it stays pending and goes out at the next sync, as
  after any interrupted sync.
- Wi-Fi is off or not yet connected: the reader asks the device to connect once for the save; saves
  made while it connects share that request. If the connection fails: no sync, no error.
- The reader taps "Sync now" while an automatic sync runs: the existing rule applies, no second
  sync starts and the reader is told one is in progress.
- A save happens while the app is showing a conflict notice or the Settings page: the save and
  the sync work as usual; the notice is not repeated.
- The setting is on and the reader forgets the account: the setting is kept, and no sync is
  attempted until a new account is set up.
- Attachments are added or removed as part of a save: they are sent by the same automatic sync as
  with a manual one.
- Many notes are edited offline and the first online save triggers the sync: it takes as long as
  a manual sync of the same changes would, and the status shows it as in progress.

## Requirements *(mandatory)*

### Functional Requirements

**Setting**

- **FR-001**: The Settings screen MUST contain a control labelled "Auto Sync" that turns
  automatic syncing after a save on or off, and MUST show its current state without opening
  anything else.
- **FR-002**: The control MUST be on by default on a fresh install and for every device that
  upgrades from an earlier version.
- **FR-003**: Changing the control MUST take effect for the next save without restarting the app
  or reopening the note.
- **FR-004**: The chosen state MUST persist across app restarts, and MUST NOT be reset by
  forgetting the account.
- **FR-005**: The control MUST be usable (and its state kept) when no account is set up; with no
  account, no sync MUST be attempted on a save.

**Triggering**

- **FR-006**: When the control is on, completing a save of a new or edited note MUST start a
  sync, provided Wi-Fi is connected and an account is set up.
- **FR-007**: When the control is on, confirming the deletion of a note MUST start a sync under
  the same conditions.
- **FR-008**: When the control is on, any other change that is saved to a note and queued for
  sync (favorite mark, category, task-list checkbox on the reading page, attachments added or
  removed with a save) MUST start a sync under the same conditions.
- **FR-009**: An automatic sync MUST be the same operation as a manual sync: it sends local
  changes first, then downloads remote ones, applies the same conflict handling and the same
  protection of unsynced notes, and reports its outcome in the same status area.
- **FR-010**: When the control is off, a save MUST NOT start a sync; the only triggers remain
  "Sync now" and the sync at app start.
- **FR-011**: Leaving the editor with unsaved changes, discarding them, or typing in the editor
  MUST NOT start a sync; only a completed save, delete or other saved change does.
- **FR-012**: The save itself MUST complete and the editor MUST respond and close at the same
  speed whether the control is on or off, and with or without a network; the sync MUST NOT block
  the interface.

**Offline, concurrency and failure**

- **FR-013**: When Wi-Fi is not connected, or the server is unreachable, a save MUST still be
  stored on the device and queued as today; when Wi-Fi is off the app MUST ask the device to connect
  once for that save and sync when it is up; a failed connection or sync MUST end without an
  error dialog, and the note MUST remain marked as waiting to sync.
- **FR-014**: The system MUST NOT run more than one sync at a time. A save that happens while a
  sync is running MUST cause one further sync to run after the current one ends, so the new
  change is not left waiting for the next manual sync.
- **FR-015**: Saves that occur close together MUST be combined so that they are sent by as few
  syncs as practical, and the system MUST NOT start one sync per save when several saves happen
  within a few seconds.
- **FR-016**: A failed automatic sync MUST show its outcome in the existing sync status area
  (what failed, when), MUST leave all notes untouched, and MUST NOT retry in a loop; the next
  trigger is the next save, "Sync now" or app start.
- **FR-017**: An automatic sync MUST NOT discard, overwrite or silently merge any version of a
  note; conflicts MUST be handled exactly as for a manual sync (both versions kept, the reader
  told).
- **FR-018**: The sync status area MUST show an automatic sync in progress, and its time and
  outcome afterwards, the same way as a manual one.

**Privacy and scope**

- **FR-019**: Automatic sync MUST send data only to the server the reader configured, over the
  same verified connection as a manual sync, and MUST NOT write note contents to the log.
- **FR-020**: The feature MUST NOT add a timer-based or background sync: a sync is started only
  by the reader's action (a manual request, app start, or a completed save with the control on).
  This refines the earlier rule that sync happens only on request or at app start by adding a
  completed save as a third, opt-in trigger.

### Key Entities *(include if feature involves data)*

- **Sync-after-saving preference**: a single on/off choice belonging to the reader of this device.
  On by default; survives restarts; independent of the account and of notes.
- **Pending change**: a locally saved create, edit, delete or flag change that has not yet been
  confirmed by the server. The automatic sync exists to shorten the time a pending change spends
  waiting; the pending-change rules themselves are unchanged.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With the setting on and Wi-Fi connected, a saved note is on the server within 30
  seconds of tapping Save in at least 95% of tries, with no other action from the reader.
- **SC-002**: A reader can find the control in Settings, change it and see its new state in under
  15 seconds, and the state is correct after a restart in 100% of tries.
- **SC-003**: With the setting off, saving, editing and deleting produce zero network activity
  from the app, identical to the behavior before this feature, in 100% of tries.
- **SC-004**: Tapping Save takes no longer than it does with the setting off, and the editor
  closes within the same time with Wi-Fi on, off, or the server down.
- **SC-005**: Across 20 saves made without Wi-Fi and then synced, no note is lost, duplicated or
  altered, and none produced an error dialog.
- **SC-006**: Ten saves made within one minute produce no overlapping syncs and no more than half
  as many syncs as saves, and all ten notes reach the server.
- **SC-007**: When the same note was changed on the server, an automatic sync keeps both versions
  in 100% of tries, as a manual sync does.

## Assumptions

- "Save" means any completed change to a note's stored content or flags: creating, editing,
  deleting, favorite, category, task-list checkbox, attachments. All of these are queued for sync
  already, so they are all covered; typing without saving is not.
- The control is a single on/off switch for the whole app; there is no per-note, per-category or
  time-window setting.
- It is on by default (decision after the first device test); a reader who wants the earlier
  behavior, sync only on request and at app start, switches it off. Refines the earlier "off by
  default" decision, which kept upgrades silent but hid the feature.
- A save made without Wi-Fi does not wait around to retry when Wi-Fi returns; it goes out at the
  next trigger (a later save, "Sync now" or app start). Retrying on a timer or on Wi-Fi coming
  back would be background sync, which stays out of scope.
- When Wi-Fi is off (the reader's usual state) a save asks the device to connect once, as "Sync now" does
  (changed after the first device test: without it the feature did nothing). A failed connection is
  not retried.
- "A few seconds" for combining close saves is on the order of 2 to 5 seconds; the exact value is a
  planning decision.
- The existing sync status area, conflict notice and Wi-Fi check are reused as they are.
- Depends on the existing sync, pending-change queue and Settings page from feature 001; no
  server-side change is needed.
