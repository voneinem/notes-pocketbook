# Bug Verification: App aborts at start (GPU device permission denied) after some use

- **Slug**: app-wont-start-after-use
- **Tested**: 2026-10-12
- **Assessment**: ./assessment.md
- **Fix**: ./fix.md
- **Result**: partial

## Summary

The launcher change behaves as designed in a host simulation and no test regressed (89 of 89 pass). The original symptom was **not** reproduced on the reader: it
appeared once, was cleared by a reboot, and the cause is still unknown. So the fix is proven to log and retry, but not proven to make the app start in the failing state.

## Checks Performed

| Check | Command / Action | Result | Notes |
|-------|------------------|--------|-------|
| Reproduction (post-fix) on the reader | start PB Notes in the failing state | not-run | the reader is no longer in that state (a reboot cleared it) and the trigger is unknown; cannot be forced from here |
| Launcher syntax | `sh -n platform/pocketbook/pbnotes.app` | pass | |
| Launcher behaviour (fake device, fake binary) | `tests/launcher/test_launcher.sh` | pass | GPU refusal then success: two runs, both results logged; refusal twice: one retry only; abort without `mali0` and normal exit: no retry; previous log kept as `pbnotes.log.prev`; diagnostics lines present |
| New test in the suite | `ctest --test-dir build-host -R launcher_script` | pass | |
| Regression suite | `ctest --test-dir build-host` | pass | 89 of 89, includes constitution, fixtures and version-bump checks |
| Version gate | `PBNOTES_BASE_REF=HEAD~1 scripts/check-version-bump.sh` | pass | VERSION 1.3.1, base 1.3.0, CHANGELOG entry present |
| Installed launcher is the repository file | `cmp platform/pocketbook/pbnotes.app <reader>/applications/pbnotes.app` | pass | byte-identical; installed 20:29 |
| Launcher on the real reader (BusyBox-like `sh`, `ps`, `stdbuf`) | start the app on the reader | not-run | needs a person at the device; the reader's log still holds a 20:22 start from the old launcher (no diagnostics lines), which says nothing about the fix |
| Lint / type-check | none configured for shell scripts | not-run | `sh -n` only |

## Output Excerpts

```
launcher tests passed
100% tests passed, 0 tests failed out of 89
version check passed (VERSION 1.3.1, base 1.3.0)
```

## Residual Risks

- The cause of the refused GPU device is unknown; if it is a persistent system state, the single retry will not help and only the log and `INSTALL.md`
  advice (restart the reader) apply.
- Not yet seen on the reader: whether `id`, `ls`, `ps`, `uptime`, `stdbuf` exist there and whether the diagnostics lines are useful; whether the retry ever succeeds.
- The reader still runs the 1.3.0 binary with the 1.3.1 launcher; a 1.3.1 ARM build has not been made.
- The suspected involvement of the app's exit path (`DeviceBridge::quit`) is neither confirmed nor excluded.

## Recommendation

Do not close the bug yet. Start PB Notes once on the reader to confirm the new launcher runs there (the log should show `--- diagnostics:` lines and a `pbnotes.log.prev`). Keep the bug open until the failure
happens again; then read both logs before restarting the reader and, if the diagnostics point at the app, re-run `/speckit-bug-assess` with that evidence. Cross-build and package
1.3.1 before tagging.
