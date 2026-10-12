# Bug Assessment: App aborts at start (GPU device permission denied) after some use

- **Slug**: app-wont-start-after-use
- **Created**: 2026-10-12
- **Source**: pasted text
- **Verdict**: likely valid, needs reproduction
- **Severity**: high

## Report (verbatim or summarized)

> After using the app for a while it does not start anymore when I click the icon from the apps menu.

Evidence read from the reader (mounted at `/run/media/voneinem/PB1030`, read only), `system/pbnotes/pbnotes.log`,
written by the failed launch at 2026-10-12 20:09:30:

```
=== pbnotes launch: Mon Oct 12 20:09:30 CEST 2026 ===
Detected locale "C" ... switched to "C.UTF-8" instead.
failed to open device file /dev/mali0 with errno 13 (Permission denied)
mali_device_info_init: Failed to init device instance 0
program_name = [/mnt/ext1/applications/pbnotes.bin], basename = [pbnotes.bin], init_string = [...]
program init string = [(null)]
default init string = [(null)]
Aborted
=== pbnotes.bin exit code: 134 ===
```

The last successful launches (`pbnotes.log.run7`, `pbnotes.log.run8`, 2026-10-11) show the normal sequence instead:
`Atached mp shm`, `epdc_rockchip_init`, `Starting task - pbnotes.app`, `OpenTheme`, then our own `ca.selected` and
`image.formats` events. No earlier log (`prev` .. `prev9`, `run7`, `run8`, `hello.log.run*`) contains `mali0` or `Aborted`,
so this is the first recorded occurrence.

## Symptom

Starting PB Notes from the Applications menu does nothing visible. The launcher script runs `pbnotes.bin`, which dies with
SIGABRT (exit code 134) before any window is created. Expected: the app starts as it did in earlier sessions.

## Reproduction

1. Use the app on the InkPad One for a while (several sessions; exact duration unknown). [NEEDS CLARIFICATION: how long, and how was the last session ended: in-app quit, Home button, or the system closing the app?]
2. Open Applications and tap the PB Notes icon.
3. Nothing appears; `system/pbnotes/pbnotes.log` shows the log above.

Not reproduced by Claude: the reader is mounted over USB and cannot be driven from here.
Reporter's answers (2026-10-12): a reboot of the reader fixes it; InkFeed (`inkfeed.app`, same launcher and QPA) does **not** start in the
failing state either (the reporter first answered the opposite and corrected it); the reader was not connected by USB; how the previous
session ended is not remembered.

## Suspected Code Paths

- `app/main.cpp:33` (`QGuiApplication app(argc, argv);`) — the first statement of `main()`. The `pocketbook2egl` platform
  plugin creates the EGL/Mali context here. The abort happens during this call, before the emoji font, the database
  (`NotesBridge`, `main.cpp:77`), `AttachmentService::purge` or any QML is touched.
- `platform/pocketbook/pbnotes.app:17-31` — the launcher. It truncates the log on every start (`exec >"$DATADIR/pbnotes.log"`),
  so the evidence of an earlier failure is lost by the next launch, and it records nothing about the process environment.
- `app/device_bridge.cpp:170-181` (`DeviceBridge::quit`) — the exit path (`SetOrientation`, `ClearScreen`, `FullUpdate`,
  `CloseApp`, then `QCoreApplication::quit`). Only a candidate for leaving the system in a bad state; no evidence yet.

Ruled out from the evidence:

- **Growing data**: `pbnotes.db` is 98 KB, `attachments/` is empty, the journal is 0 bytes (normal for `journal_mode=TRUNCATE`).
  The abort happens before the database is opened anyway.
- **Launcher or binary changed**: `pbnotes.app` is identical to InkFeed's apart from names; the binary is the one that
  started fine on 2026-10-11 (installed 22:17).

## Root Cause Hypothesis

The process the system started for the icon had no permission to open `/dev/mali0` (EACCES, not EBUSY). The Mali user-space driver
then failed to initialise, the Qt EGL platform plugin could not create a display, and Qt aborted inside `QGuiApplication`.

The reporter's answers narrow the cause:

- InkFeed fails in the same state, so every Qt/EGL app on the reader is affected. The reader's system lost GPU access for newly started
  apps; this is not specific to PB Notes, its launcher, its data or its code.
- A reboot clears it, so it is runtime state of the reader (device node permissions, the GPU driver or the launching service), not anything
  stored on the card.
- USB mode is ruled out.

What is not known is what puts the reader into this state "after a while". Candidates, none observed:

1. A system-side event on the reader (firmware service restart, permission reset of `/dev/mali0`) unrelated to our apps. Then PB Notes is only
   the app the reporter noticed, and there is nothing to fix in the repository except diagnostics and documentation.
2. Our app contributes: an exit through `DeviceBridge::quit()` (`app/device_bridge.cpp:170-181`: `ClearScreen`, `FullUpdate`, `CloseApp()`, then
   `QCoreApplication::quit()`), or a process that lingers after `CloseApp()` while `NotesBridge::~NotesBridge()` (`bridge/src/notes_bridge.cpp:76-79`)
   joins a sync thread (up to 60 s for notes, 600 s for attachments, `core/include/pbnotes/http_transport.h:28`, `core/src/api_client.cpp:380,426`),
   leaves the GPU stack in a bad state. InkFeed uses the same exit pattern (the code was adapted from it), so this would also be possible with
   InkFeed alone; it cannot be told apart from (1) with the evidence we have.

Confidence that the note logic (database, Markdown, QML) is not the trigger: high. Confidence in (1) versus (2): low; the next failure needs
better evidence (see Remediation).

## Proposed Remediation

**Preferred**: evidence first, in the launcher only (no change to the app logic). The current launcher truncates its log on every start and
records nothing about the system, so a failed start leaves no trace of why the GPU was unreachable. In `platform/pocketbook/pbnotes.app`:

1. Keep the previous log (`mv pbnotes.log pbnotes.log.prev` before `exec >`), so a failed launch no longer erases the one before it.
2. Before starting the binary, log facts that separate the candidates: `id`, `ls -l /dev/mali0`, the running `pbnotes.bin` / `inkfeed.bin`
   processes, `uptime`, `date`, free memory. Every command must tolerate failure (`2>/dev/null || true`).
3. If the binary exits with 134 and the output contains `mali0`, log it as a distinct line, wait about 2 s and start it once more (one retry),
   and log both results. A transient condition then recovers by itself; a persistent one is recorded.
4. Make the app's own start-up lines survive an abort (line-buffered stdout or stderr), which hides how far startup got today.
5. `INSTALL.md` Troubleshooting: if the log shows `/dev/mali0 ... Permission denied`, restart the reader; this affects other Qt apps too.

Whether to also shorten the exit path (candidate 2) is decided from the first log that shows the state of `/dev/mali0` and any leftover
process. Without that evidence, changing `DeviceBridge::quit` risks the black-framebuffer problem it was written to avoid.

**Alternatives**:
- Change the exit path to leave through `exit()` only, without `CloseApp()`. Risky: `CloseApp()` is there to avoid a black framebuffer
  (see the comment in `DeviceBridge::quit`), and there is no evidence yet that it is the cause.
- Do nothing in the app and document "restart the reader" in `INSTALL.md` troubleshooting. Cheap, but the cause stays unknown.

**Files likely to change**:
- `platform/pocketbook/pbnotes.app`
- `INSTALL.md` (Troubleshooting: how to read the log, restart as the first remedy)
- `CHANGELOG.md`, `VERSION` (the repository checks a version bump, see `scripts/check-version-bump.sh`)

**Tests to add or update**:
- A host test or script check that the launcher stays valid POSIX `sh` (`sh -n`) and still keeps the previous log.
- Manual, on the reader: normal start; start with `pbnotes.bin` already running (stale instance); a forced abort (for example start with
  `QT_QPA_PLATFORM` pointing to a missing plugin) to confirm the retry and both logs are written.

## Risks & Considerations

- The launcher runs as the reader's app user with a minimal toolset (BusyBox-like `sh`): `pgrep`, `id`, `uptime` may be missing. Every
  diagnostic command must tolerate failure (`2>/dev/null || true`).
- A blind retry could hide the problem or loop; limit it to one retry.
- If the cause is the reader's system (candidate 1), the retry will not help and the launcher can only record it. Say so in the changelog
  instead of presenting the retry as a fix.
- Logs must not contain personal data (constitution Principle III). The proposed diagnostics contain none.
- The launcher is not covered by the host build or CTest; verification needs the device.

## Open Questions

- [NEEDS CLARIFICATION: Was a sync or attachment transfer running when the last session ended? Not remembered; ask again at the next failure.]
- [NEEDS CLARIFICATION: On the next failure, what do `ls -l /dev/mali0` and the process list show? The launcher diagnostics will answer it.]
- [NEEDS CLARIFICATION: Has any other app than PB Notes and InkFeed (for example the built-in reader) been affected in this state? If only the Qt/EGL apps fail, the GPU node is the cause.]
