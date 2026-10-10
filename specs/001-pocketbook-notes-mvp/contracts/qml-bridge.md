# Contract: QML bridge (`bridge` context property)

Thin Qt layer between `notes_core` and QML. It contains no sync or storage logic; each member
delegates to the core. Used by the single `ApplicationWindow` (`qml/Main.qml`).

## Properties (read, with change notification)

| Name | Type | Meaning |
|------|------|---------|
| `configured` | bool | an account exists |
| `orientation` | int | InkView orientation 0..3 currently applied (informational; the system and Qt rotate the window, the UI does not rotate itself) |
| `fontPx` | int | reader font pixel size for the chosen small/medium/large |
| `fontSize` | string | `small`/`medium`/`large` |
| `syncRunning` | bool | a sync is in progress |
| `syncStatus` | string | translated one-line status (time + outcome), always non-empty |
| `syncOutcome` | string | `never`/`running`/`success`/`partial`/`failed`/`offline`/`maintenance` |
| `conflictCount` | int | conflicts in the last sync |
| `pendingCount` | int | notes with unsynced changes |
| `view`, `sortOrder` | string | current list view and sort |
| `categories` | list of {name,count} | for the view picker |
| `notes` | model | rows: `localId`, `title`, `category`, `modified`, `favorite`, `pending`, `excerpt` |
| `searchText` | string | substring filter on title and content |
| `caSource` | string | `device` or `bundled`, shown in the connection test and in Settings |
| `customCaStatus` | string | `none`, `loaded` or `invalid`, shown in Settings |
| `customCaCertificates` | int | number of certificates read from `custom-ca.pem` (0 unless `loaded`), shown in Settings |

## Invokables

| Name | Purpose |
|------|---------|
| `testConnection(url, user, password)` | async; emits `connectionTested(ok, errorClass, message)` |
| `saveAccount(url, user, password)` | persist after a successful test; starts the first sync |
| `forgetAccount()` | removes credentials (overwrites the password) after UI confirmation |
| `syncNow()` | starts a sync if none is running |
| `setView(view)`, `setSortOrder(o)`, `setSearch(text)` | list state |
| `openNote(localId)` → blocks | model of rendered blocks for the reader (`markdownBlocks`) |
| `toggleTask(localId, lineIndex)` | flips a checkbox, saves as a normal edit |
| `loadForEdit(localId)` → map | `{text, category, favorite}` for the editor (empty map if unknown) |
| `saveNote(localId, text, category, favorite)` → localId | explicit save; `-1` for a new note; empty new notes are rejected |
| `deleteNote(localId)` | after UI confirmation |
| `setCategory(localId, name)`, `toggleFavorite(localId)` | normal local edits |
| `setFontSize(s)`, `setRotationPreference(p)` | persisted settings |
| `quit()` | `ClearScreen(); FullUpdate(); CloseApp();` on device, as in InkFeed, then `QCoreApplication::quit()` |

## Signals

`connectionTested`, `syncFinished(outcome, message)`, `conflictsDetected(count)`,
`noteChanged(localId)`, `errorOccurred(message)`.

## UI rules the bridge must support

- No call blocks the UI thread for network or sync; DB reads for list/search must stay within the
  performance goals (3 s open, 2 s search at 1,000 notes).
- Every user-facing string is looked up through one translation function so a language file can be
  added later (English only in the MVP).
- The bridge never exposes the password to QML after `saveAccount`.
