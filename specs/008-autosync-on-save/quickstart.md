# Quickstart: validating Autosync on Save

Validation guide only; implementation details live in `tasks.md`. Contracts:
[core-library](contracts/core-library.md), [qml-bridge](contracts/qml-bridge.md). Data:
[data-model](data-model.md). Decisions: [research](research.md). Build, packaging and device
prerequisites are those of `specs/001-pocketbook-notes-mvp/quickstart.md` §1–§3.

## 1. Host build and tests (no device)

```bash
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host
ctest --test-dir build-host --output-on-failure
scripts/pre-merge.sh
```

Expected: all suites pass offline. New: `test_sync_scheduler`, `settings_sync.qml`. Extended:
`test_store_open`, `test_account_store`, `test_store_migration_v3`, `test_store_edit`, `test_notes_bridge`, `editor_flow.qml`, `stubs.qml`. Each is
written first and observed failing before its implementation (constitution I). `app_self_test`
must still pass.

## 2. Host scenarios (what the tests prove)

| Scenario | Spec | Where |
|----------|------|-------|
| quiet period, gap, combine, follow-up, drop offline / nothing pending / off / no account, editor deferral, ten saves → ≤ 5 starts | US3-1..5, FR-013..FR-015, SC-006 | `test_sync_scheduler` |
| `sync_after_save` default on, accepts on/off only, survives forget account | US2-1, 2-5, FR-002, FR-004 | `test_store_open`, `test_account_store`, `test_notes_bridge` |
| switch on + save reaches the local test server without `syncNow()`; switch off → no request | US1-1, US2-2, SC-003 | `test_notes_bridge` |
| create, delete, favorite, category, checkbox each trigger | US1-2..4, FR-007, FR-008 | `test_notes_bridge` |
| offline callback → no request, no `errorOccurred`, note still pending | US3-1, FR-013 | `test_notes_bridge` |
| save while a sync runs → exactly one more sync afterwards | US3-4, FR-014 | `test_notes_bridge` |
| failed automatic sync (server down) → status says why, no retry, no dialog | US3-3, FR-016 | `test_notes_bridge` |
| editor open → no start; closed → start | FR-011 (deferral), research R-6 | `test_notes_bridge` |
| stale editor text after a pull → conflict copy, both versions kept | FR-017, research R-6 | `test_store_edit`, `test_notes_bridge` (`saving_stale_editor_text_after_a_pull_keeps_both_versions`) |
| Settings button shows and flips the state; editor reports open/closed | US2-1, 2-3 | `settings_sync.qml` |

## 3. Device walkthrough (InkPad One)

Prerequisite: an account set up and a few synced notes; a second client (phone or browser) on the same
server to see results. Install the build as for earlier features.

| # | Do | Expect |
|---|----|--------|
| 1 | Open Settings | "Synchronization" section, "Auto Sync: on" |
| 2 | Switch it off, edit a note and Save, Wi-Fi on | Note saved, list shows it as waiting; the server does **not** change; no network use |
| 3 | Switch it on again, leave Settings, edit a note and Save | Editor closes immediately; within about 30 s the server shows the new text; the Sync now button reads "Syncing..." from the save until the sync ends; status line shows "Syncing..." then the new time and success |
| 4 | Create a note, then delete another and confirm | Both reach the server within about 30 s |
| 5 | Tick a task box on the reading page; mark a favorite; change a category | Each reaches the server (together if done within a few seconds) |
| 6 | Turn Wi-Fi off, save a note | Saved at once, no error dialog; the reader switches Wi-Fi on by itself and within about 30 s the note is on the server (device log: `autosync.connect`, `autosync.network result=connected`, `autosync.start`) |
| 7 | Switch Wi-Fi off and make it unable to connect (out of range), save a note | No error dialog; the note stays waiting (`autosync.network result=failed`, `autosync.drop reason=offline`); it goes out with the next sync |
| 8 | Save two notes 3 s apart | Both arrive; the status shows a single sync, not two |
| 9 | Save, then at once open "Sync now" | No second overlapping sync; the button says "Syncing..." |
| 10 | Edit the same note on the server and (after saving) on the reader while online | The conflict dialog appears once; both versions exist |
| 11 | Save, then within 3 s open another note in the editor and wait 20 s | No sync starts while the editor is open; after leaving it, the pending changes go out |
| 12 | Switch it off and restart the app | Still off after the restart; saves no longer sync |
| 13 | Switch it on again, restart the app | Still on; with Wi-Fi up, the start-up sync still runs once |

Record results in a walkthrough note in this folder, as for earlier features.
