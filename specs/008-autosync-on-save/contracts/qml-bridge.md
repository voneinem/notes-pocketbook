# Contract: QML bridge and controls

Extends `specs/006-markdown-slash-menu/contracts/qml-bridge.md`. No new context property.

## `NotesBridge` additions

| Member | Behavior |
|--------|----------|
| `syncAfterSave` (property, bool, read/write, `syncAfterSaveChanged`) | reads/writes the persisted setting `sync_after_save`; writing the current value does nothing; turning it off disarms the scheduler; turning it on starts nothing. Works without an account |
| `setEditorOpen(bool)` (`Q_INVOKABLE`) | the editor page reports that it is open / closed; closing re-asks the scheduler. Counted (two editors never exist, but the call is idempotent) |
| `Config::wifiConnected` (`std::function<bool()>`) | empty = online; set by `app/main.cpp` to `DeviceBridge::wifiConnected` |
| `Config::autoSyncQuietMs`, `Config::autoSyncGapMs` | scheduler options; default 4000 / 15000; tests set small values |
| `saveNote(...)` (changed) | for an existing note that `loadForEdit` returned: if the stored text, category or favorite differ from what the editor loaded, saves the text as a new note with the conflict suffix, returns **that** note's id, emits `conflictsDetected(1, 0)`; otherwise unchanged |
| `loadForEdit(...)` (changed) | also remembers the note's text, category and favorite as the editing base; `setEditorOpen(false)` forgets all bases |

All six edit methods (`saveNote`, `deleteNote`, `setCategory`, `toggleFavorite`, `toggleTask`, and the
attachment binding done inside `saveNote`) call the scheduler after their store call succeeded and
before the function returns; none waits for the network. `syncNow()` (existing) additionally calls
`sync_started` for every sync it begins. When a sync ends, the bridge re-asks the scheduler after
`syncFinished` (this is how the single follow-up runs).

Existing signals are reused as they are: `syncStateChanged`, `notesChanged`, `noteChanged`,
`syncFinished`, `conflictsDetected`. No new signal except `syncAfterSaveChanged`.

## Settings page

A new section titled "Synchronization" above "Account":

- One `InkButton` labelled `Auto Sync: on` / `Auto Sync: off`, toggling
  `bridge.syncAfterSave` on click (same pattern as the rotation button). Always enabled.
- One line of explanatory text under it (28 px, like the certificate text): "When on, the reader
  syncs a few seconds after you save a change, switching Wi-Fi on if needed. When off, notes sync only with
  Sync now and at start."

## Editor page

`Component.onCompleted: bridge.setEditorOpen(true)`, `Component.onDestruction: bridge.setEditorOpen(false)`.
No other change: Save still writes and closes at once.

## Test stubs (`tests/qml/stubs.qml`)

The `bridge` stub gains `syncAfterSave` (bool, writable) and `setEditorOpen(open)` (records calls).
`settings_sync.qml` asserts: the button text follows the property, a click flips it, and the editor
page calls `setEditorOpen(true)` on open and `false` on destruction.

## Log events (codes and counts only; never note text)

`autosync.armed`, `autosync.start pending=<n>`, `autosync.drop reason=<code>`,
`autosync.deferred reason=<code>`, `note.conflict_on_save` (count only).
