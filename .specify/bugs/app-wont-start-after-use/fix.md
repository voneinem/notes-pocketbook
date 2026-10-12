# Bug Fix: Launcher diagnostics and one retry for the refused GPU device

- **Slug**: app-wont-start-after-use
- **Fixed**: 2026-10-12
- **Assessment**: ./assessment.md
- **Status**: partial

## Summary

The reader sometimes refuses new apps access to `/dev/mali0` until it is restarted (also InkFeed). The cause is in the reader's system and
is not removed. The launcher now keeps the previous log, records the system state at every start, keeps the app's start-up lines when it aborts, and
retries once after exactly this refusal, so the next failure leaves usable evidence and may recover by itself. The status is `partial` because the
cause is still unknown and the retry is unproven on a real failure.

## Changes

| File | Change | Notes |
|------|--------|-------|
| `platform/pocketbook/pbnotes.app` | modified | keeps `pbnotes.log.prev`; logs uid/groups, `/dev/mali0`, uptime, memory, running app processes; line-buffered output via `stdbuf` when present; one retry after exit 134 with `mali0` in this attempt's output |
| `tests/launcher/test_launcher.sh` | added | runs the launcher against a fake device layout and fake binary |
| `tests/CMakeLists.txt` | modified | registers `launcher_script` |
| `INSTALL.md` | modified | troubleshooting: what the `mali0` message means, `pbnotes.log.prev`, restart the reader |
| `CHANGELOG.md`, `VERSION` | modified | 1.3.1; the entry says plainly that the cause is not fixed |

## Diff Highlights

```sh
[ -f "$DATADIR/pbnotes.log" ] && mv -f "$DATADIR/pbnotes.log" "$DATADIR/pbnotes.log.prev" 2>/dev/null
exec >"$DATADIR/pbnotes.log" 2>&1
...
mark=$(wc -c < "$DATADIR/pbnotes.log" 2>/dev/null || echo 0)
run_app; code=$?
if [ "$code" -eq 134 ] && tail -c +"$((mark + 1))" "$DATADIR/pbnotes.log" | grep -q 'mali0'; then
    sleep 2; run_app
fi
```

## Tests Added or Updated

- `tests/launcher/test_launcher.sh` (CTest `launcher_script`) — launcher is valid `sh`; GPU refusal then success logs both results; two refusals give exactly
  one retry; an abort without `mali0` is not retried; a normal exit is not retried; the previous log is kept as `pbnotes.log.prev`.

## Local Verification

- `sh -n platform/pocketbook/pbnotes.app` → ok.
- `tests/launcher/test_launcher.sh` → "launcher tests passed".
- `cmake --build build-host && ctest` in `build-host` → 89 of 89 passed (includes `launcher_script`, the version-bump and constitution checks).
- Manual: copied the launcher to the mounted reader (`applications/pbnotes.app`, byte-identical to the repository file). Not started on the reader: that
  needs a person at the device.

## Deviations from Assessment

- The assessment listed `app/device_bridge.cpp` and the bridge only as a possible later step; they are unchanged, as recommended.
- The ARM binary on the reader is still the 1.3.0 build. `VERSION` is 1.3.1, so a release zip needs a rebuilt binary
  (`scripts/package-device.sh` refuses a version mismatch). The launcher works with either binary.
- `stdbuf` and `ps` may be missing on the reader; the launcher falls back silently, so whether the buffered start-up lines are kept on a
  real abort is not yet confirmed.

## Follow-ups

- Cross-build the ARM binary as 1.3.1 and package it before tagging `v1.3.1`.
- At the next failure, before restarting the reader: read `system/pbnotes/pbnotes.log` and `.log.prev`; check the diagnostics lines (`gpu node`, `other app processes`)
  and whether the retry ran. Decide from them whether the exit path (`DeviceBridge::quit`) is involved.
- Check whether the built-in reader or other system apps also fail in that state.
