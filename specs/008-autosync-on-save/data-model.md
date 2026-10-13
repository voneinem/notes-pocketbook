# Data Model: Autosync on Save

**One setting row is added; no table, column, file or sync field changes.** The schema stays at the
version of feature 006. Notes, the pending-change queue and the sync protocol are untouched.

## Stored: the preference (`settings` table, existing key/value rows)

| Key | Values | Default | Notes |
|-----|--------|---------|-------|
| `sync_after_save` | `on`, `off` | `on` (returned when the row is absent) | Any other value is refused by the store. Not touched by `forget_account`. Read on every decision, so a change applies to the next save (FR-003) |

Upgrading from any earlier version needs no migration: the row does not exist and reads as `on`
(FR-002).

## In memory: scheduler state (`SyncScheduler`, lives as long as the bridge)

| Field | Meaning |
|-------|---------|
| `armed` | a save happened that no sync has started after yet |
| `last_save_ms` | time of the latest save (monotonic clock, milliseconds) |
| `last_start_ms` | time the latest sync of any kind started; none yet = no gap applies |

Nothing is persisted: after a restart the app has no armed state, and the start-up sync (existing)
sends whatever is pending (edge case "closed a few seconds after saving").

### Inputs given to `next(...)` (read fresh each time, never cached)

| Input | Source |
|-------|--------|
| `enabled` | `sync_after_save == "on"` |
| `configured` | an account exists |
| `running` | a sync is running (`syncRunning_`) |
| `editor_open` | the editor page told the bridge it is open (`setEditorOpen`) |
| `has_pending` | the store lists at least one note with unsynced changes |
| `online` | `Config::wifiConnected()` (empty callback = online) |

### States and transitions

```text
          note_saved                    sync_started(t >= last_save)
 IDLE ───────────────► ARMED ─────────────────────────────────────────► IDLE
   ▲                     │  next(): off / no account / nothing pending / offline
   │                     └──────────────────────────────────────────────► IDLE (dropped, no retry)
   │                     │  next(): running or editor open  → stays ARMED, asked again later
   │                     │  next(): quiet period or gap not over → Wait(ms), stays ARMED
   │                     └─ next(): all clear → Start → bridge calls syncNow() → sync_started → IDLE
 note_saved while a sync runs: ARMED (last_save > the running sync's start) → one follow-up when it ends
```

`ARMED` is re-evaluated when: a timer fires, a sync ends, the editor closes, the switch changes, or
another save happens.

## In memory: the editor's base version (`NotesBridge`)

| Field | Meaning |
|-------|---------|
| `editBase_[localId]` | the text, category and favorite mark of the note when `loadForEdit` returned it (an `EditBase`); absent for new notes and for notes never loaded in an editor |

Removed when the note is saved, deleted or the editor reports closed. A save compares it with the stored
note (research R-6); a difference in text, category or favorite makes the editor's text a conflict copy and
never overwrites. A new ETag with the same text is not a difference.

## Unchanged entities

- **Pending change**: a note with `status = Edited/Deleted` or never uploaded; the scheduler only reads
  whether the list is empty.
- **Sync result / status**: produced and shown by the existing engine and status line.
