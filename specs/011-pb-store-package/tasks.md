---

description: "Task list for PB Store Release Package"
---

# Tasks: PB Store Release Package

**Input**: Design documents from `/specs/011-pb-store-package/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/packaging.md, quickstart.md

**Tests**: REQUIRED. The constitution makes Test-First non-negotiable (Principle I): every test task below MUST be written
and observed FAILING before the implementation task it names (a test that calls a script option or file that does not exist
yet counts as failing). The exception is marked *guard*: a test that already passes today and protects behavior that must not
change.

**Organization**: Tasks are grouped by user story (US1 … US4 of spec.md). Paths are relative to the repository root. This
feature changes build/release scripts and docs only: **no file under `app/`, `core/`, `bridge/`, `qml/` or `res/` changes**
(FR-015).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US4 as in spec.md
- **[MANUAL]**: needs a person, the ARM build, the reader or a publishing step; an implementing agent skips these and
  reports them as open

## Conventions that apply to every task

- Bash with `set -euo pipefail`, the style of `scripts/package-device.sh` and `scripts/check-version-bump.sh` (functions,
  short comments, messages on stderr, `exit 1`). No new language, no new library.
- **No absolute local paths** in any tracked file (`scripts/check-constitution.sh` R5), including docs and examples: write
  `<path to the tool>`, never a real directory. **No tool location** anywhere but `PATH` or `PBSTORE_CATALOG`.
- Test doubles for `pbstore-catalog` are small shell scripts created in the test's temp dir. The real tool is used only
  where a task says so, found by the same lookup as the script (`PBSTORE_CATALOG`, else `PATH`); when it is absent that part
  of the test prints `SKIP` and the test exits 77 only if *nothing* else ran (CTest `SKIP_RETURN_CODE 77`).
- The fake ARM "binary" is a file containing the text of `VERSION` (the script only greps it).
- Fixed manifest values are exactly those of `data-model.md`; quote them from there, do not retype from memory.

---

## Phase 1: Setup

- [x] T001 Record the baseline in `specs/011-pb-store-package/research.md` under a new heading "Baseline (1.10.0)": run
  `pbstore-catalog check-package dist/pbnotes-inkpad-one-1.10.0.zip` and paste its output (expected: exactly `E-PKG-04
  INSTALL.md` and `E-PKG-05 manifest.json`), and `unzip -Z1` of that ZIP
- [x] T002 [P] Create `tests/packaging/applications.expected`: the sorted list of file paths under `applications/` in
  `dist/pbnotes-inkpad-one-1.10.0.zip` (files only, no directory entries, no `INSTALL.md`; 17 lines: the 5 top-level
  files `pbnotes.app`, `pbnotes.bin`, `pbnotes-cacert.pem`, `pbnotes-emoji.ttf`, `pbnotes-emoji-OFL.txt`, the 2 icons, and
  the `pbnotes-licenses/` files). This is the SC-003 reference

## Phase 2: Foundational (blocks every story)

- [x] T003 Create `packaging/manifest.template.json` with exactly the values of the "Manifest template" table in
  `data-model.md` (`schema_version` "1.0", `id` "pbnotes", `name` "PB Notes", `version` "@VERSION@", `min_firmware`
  "6.11.1437", `target_device` "InkPad One", `menu_entry` {`id` "U_PbNotes", `title` "PB Notes", `path`
  "applications/pbnotes.app", `icon` "applications/icons/pbnotes.bmp", `focused_icon` "applications/icons/pbnotes_f.bmp",
  `group` "@General"}, `data_dir` "system/pbnotes"); no `files` key; valid JSON (check with `python3 -m json.tool`)
- [x] T004 Create the test harness `tests/packaging/test_package_device.sh` (executable): `ROOT` from `$1` or the script's
  location; a temp dir with trap cleanup; `check`/`fail` helpers like `tests/launcher/test_launcher.sh`; a helper
  `build <out-dir> [env…]` that creates the fake binary (text of `VERSION`) and runs `scripts/package-device.sh <fake> ""
  <out-dir>` capturing stdout, stderr and the exit code; a helper `real_tool` that prints the real tool path or nothing;
  a helper `stub <name> <body>` that writes an executable stub `pbstore-catalog` into a temp bin dir. No test cases yet
  (the file runs and exits 0)
- [x] T005 Register the test in `tests/CMakeLists.txt` next to `launcher_script`:
  `add_test(NAME packaging_script COMMAND ${CMAKE_SOURCE_DIR}/tests/packaging/test_package_device.sh ${CMAKE_SOURCE_DIR})`
  and `set_tests_properties(packaging_script PROPERTIES SKIP_RETURN_CODE 77 LABELS "script")`

**Checkpoint**: `ctest -R packaging_script` runs and passes with no cases.

---

## Phase 3: User Story 1 - The release package is accepted by the PB Store contract check (Priority: P1) 🎯 MVP

**Goal**: the built ZIP has `manifest.json` and `applications/` at its root only and passes `check-package`.

**Independent Test**: with the real tool available, run `scripts/package-device.sh` on a fake binary, then
`pbstore-catalog check-package` on the ZIP: exit 0; `unzip -Z1` shows only `manifest.json` and `applications/…`; the manifest
shows the data-model values and the `VERSION`.

### Tests for User Story 1 (write first, observe failing)

- [x] T006 [US1] In `tests/packaging/test_package_device.sh` add case "package passes the store check" (real tool, else
  SKIP): build into a temp dir **with the real tool's directory prepended to `PATH`** (until US2 the script looks the tool up
  on `PATH` only, even when the test found it through `PBSTORE_CATALOG`), then `<real tool> check-package <zip>` exits 0.
  Observed failing: the current script puts
  `INSTALL.md` at the root and writes no manifest (E-PKG-04, E-PKG-05). Run it and record the failure in the task's commit message
- [x] T007 [US1] In the same file add case "top level and manifest" (needs only a **stub** tool, so it always runs): the stub
  `pbstore-catalog` implements `pack <dir> --out <zip>` by copying the whole `<dir>` tree to a side directory and creating an
  empty file at `<zip>` (no ZIP tool needed), and `check-package` as `exit 0`. Assert on the **staged directory** the stub saw: `manifest.json` exists, its
  `version` equals `cat VERSION`, `schema_version` "1.0", `id` "pbnotes", `name` "PB Notes", `min_firmware` "6.11.1437",
  `target_device` "InkPad One", `menu_entry.id` "U_PbNotes", `title` "PB Notes", `path` "applications/pbnotes.app", `icon`
  "applications/icons/pbnotes.bmp", `focused_icon` "applications/icons/pbnotes_f.bmp", `group` "@General", `data_dir`
  "system/pbnotes"; the staged top level is only `manifest.json` and `applications`; no `INSTALL.md` anywhere. (Parse with
  `python3 -c 'import json,sys…'` or `grep`, whichever the file already uses.) Observed failing: no manifest is staged today
- [x] T008 [US1] In the same file add case "version comes from one source": run `build` with a fake binary containing `9.8.7`
  and the version argument `9.8.7`; the ZIP is named `pbnotes-inkpad-one-9.8.7.zip` and
  the staged manifest says `9.8.7`. Observed failing for the manifest part

### Implementation for User Story 1

- [x] T009 [US1] Edit `scripts/package-device.sh`: (a) add `$ROOT/packaging/manifest.template.json` to the required-input
  list and remove `$ROOT/INSTALL.md` from it; (b) delete the `install … INSTALL.md` line; (c) after staging
  `applications/`, write `$stage/manifest.json` with `sed "s/@VERSION@/$VERSION/g" "$ROOT/packaging/manifest.template.json"`;
  (d) replace `zip -r -q` by `pbstore-catalog pack "$stage" --out "$ZIP"` followed by `pbstore-catalog check-package "$ZIP"`,
  the tool taken from `PATH` as a bare name for now (US2 adds the lookup), the tool's stdout redirected to stderr so stdout
  stays the ZIP path; (e) update the header comment: layout without `INSTALL.md`, with `manifest.json` created at build time
- [x] T010 [US1] Run `ctest -R packaging_script --output-on-failure`: T006–T008 now pass (T006 with the real tool). Run
  `scripts/package-device.sh` on a fake binary by hand and `unzip -Z1` the result to see the layout

**Checkpoint**: US1 delivers a store-valid package. Stop here for a minimal release if needed.

---

## Phase 4: User Story 2 - Reproducible build with the store's tool, or a clear failure (Priority: P2)

**Goal**: tool lookup through `PBSTORE_CATALOG` / `PATH`, a clear message when it is missing, a reproducibility proof on every
build, and no new ZIP after any failure.

**Independent Test**: build twice and compare; build with no tool and read the message.

### Tests for User Story 2 (write first, observe failing)

- [x] T011 [US2] In `tests/packaging/test_package_device.sh` add case "tool missing": `PATH` set to a temp dir holding only
  symlinks to the commands the script needs (`env` for the `#!/usr/bin/env bash` line, `bash`, `sed`, `grep`, `head`, `tr`,
  `sort`, `mktemp`, `install`, `mkdir`, `rm`, `cmp`, `cat`, `dirname`, `basename`, `mv`; `cd` is a builtin) and `PBSTORE_CATALOG` unset: exit code 1, stderr contains
  `pbstore-catalog not found` and `PBSTORE_CATALOG`, **no** `*.zip` in the output folder, and the check happens before any
  staging (stderr has no `missing input`). Observed failing: today the shell reports `command not found` with another exit code
  and text
- [x] T012 [US2] Add case "environment variable": a stub tool at `<tmp>/tools/mytool` (not named `pbstore-catalog`),
  `PBSTORE_CATALOG=<tmp>/tools/mytool`, `PATH` without any tool: the build succeeds and the stub was called (it appends to a
  log). Add a second assertion: `PBSTORE_CATALOG=<tmp>/nonexistent` with a good tool on `PATH` exits 1 with `PBSTORE_CATALOG
  is set but is not an executable file` and does **not** fall back to `PATH`. Observed failing
- [x] T013 [US2] Add case "reproducibility": (a) a stub whose `pack` writes the current nanosecond time into the ZIP (so two
  packs differ): exit 1, stderr `package is not reproducible`, no ZIP in the output folder; (b) a stub whose `pack` is
  deterministic: exit 0, and `pack` was called twice (log). Observed failing: the script packs once
- [x] T014 [US2] Add case "check failure leaves nothing": a stub whose `check-package` prints
  `error[E-PKG-04] x: y` to stderr and exits 1: build exits 1, that line is on the build's stderr, `package check failed`
  follows, and the output folder contains no `*.zip` — also when a ZIP of the same version existed before (create a
  placeholder file with that name first; it must be gone). Observed failing
- [x] T015 [US2] Add case "bad inputs": version argument `1.2` or `1.2.x` → exit 1 `version is not MAJOR.MINOR.PATCH`; a
  template with an extra `@X@` needs a different `ROOT` (the script derives it from its own location), so build a temporary
  root: copy `scripts/` and `packaging/` from the repository into it, symlink `platform`, `third_party`, `res`, `licenses`, `LICENSE`
  and `VERSION` from the repository, append `@X@` to the **copied** `packaging/manifest.template.json`, and run the copied
  script → exit 1 `unfilled placeholder`; a missing template (delete it in that copy) → `missing input: …manifest.template.json`. Observed
  failing for the first two

### Implementation for User Story 2

- [x] T016 [US2] Edit `scripts/package-device.sh`: add `resolve_tool` (R-2): if `PBSTORE_CATALOG` is set (even empty →
  treat empty as unset) require `[ -f … ] && [ -x … ]` else print the "is set but is not an executable file" message and exit 1;
  otherwise `command -v pbstore-catalog` else print the "not found" message of `contracts/packaging.md` and exit 1. Call it
  right after argument handling, **before** the input checks. Use `"$PBSTORE"` everywhere the tool is run
- [x] T017 [US2] Edit `scripts/package-device.sh`: validate the version with `[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]`
  (message from the contract) before it is used; after writing the manifest, fail with `manifest template has an unfilled
  placeholder: <text>` if `grep -Eo '@[A-Z_]+@'` finds one
- [x] T018 [US2] Edit `scripts/package-device.sh`: replace the single pack by R-4/R-5: `rm -f "$ZIP"`; pack to
  `$work/a.zip` and `$work/b.zip` (`$work` inside the existing temp dir, outside `$stage`); `cmp -s` them, else the
  not-reproducible message; `check-package "$work/a.zip"`, on failure print `package check failed` and exit 1; `mv
  "$work/a.zip" "$ZIP"`; print `$ZIP` on stdout. Redirect the tool's stdout to stderr throughout
- [x] T019 [US2] Run `ctest -R packaging_script --output-on-failure`: T011–T015 pass. Run the real tool by hand through
  the quickstart §2 table rows "Reproducible" and "Store contract"

**Checkpoint**: US1 + US2 give a trustworthy, reproducible build.

---

## Phase 5: User Story 3 - Manual installation keeps working, instructions stay reachable (Priority: P2)

**Goal**: the files under `applications/` are exactly the 1.10.0 set; the instructions live in the repository and the release
notes, with no copy in the ZIP.

**Independent Test**: compare the file list with `tests/packaging/applications.expected`; follow the README to `INSTALL.md`.

### Tests for User Story 3

- [x] T020 [US3] In `tests/packaging/test_package_device.sh` add case "applications set unchanged" (*guard*; uses the stub from
  T007, which records the staged tree): the sorted list of staged files under `applications/` equals
  `tests/packaging/applications.expected`. This passes already after US1; keep it so a later change to the file set fails
  loudly. Also assert the staged tree has no file named `INSTALL.md` (FR-005)
- [x] T021 [P] [US3] Create a doc check inside the same test file: `README.md` contains the link `](INSTALL.md)` and does
  **not** contain `also part of the package`; `INSTALL.md` contains `merging it with the existing` (the manual steps
  survive). Observed failing: the README sentence still says "(also part of the package)"

### Implementation for User Story 3

- [x] T022 [P] [US3] Edit `README.md` "Installing on the device": reword to "See [INSTALL.md](INSTALL.md) (in the
  repository, linked from each GitHub release; it is not inside the package)" and keep the in-short sentence
- [x] T023 [P] [US3] Edit `INSTALL.md`: the first paragraph says "the release zip" and "Unzip the package" — keep, and add one
  sentence that the package also contains a `manifest.json` at its root (read by the PB Store, ignored here: copy only the
  `applications/` folder). Nothing else changes (FR-011)
- [x] T024 [US3] Run `ctest -R packaging_script`: T020, T021 pass

**Checkpoint**: manual installers lose nothing.

---

## Phase 6: User Story 4 - The release is tagged and published with the package attached (Priority: P3)

**Goal**: a local gate that the tag, `VERSION`, ZIP name and manifest version agree, and the documented release steps.

**Independent Test**: `scripts/check-release.sh --self-test`; then on a real package `scripts/check-release.sh`.

### Tests for User Story 4 (write first, observe failing)

- [x] T025 [US4] Create `scripts/check-release.sh` containing **only** the `--self-test` branch and a stub `check()`
  (header comment per `contracts/packaging.md`), in the style of `scripts/check-version-bump.sh`. The self-test builds
  throw-away ZIPs in a temp dir with `python3 -m zipfile -c <zip> <files…>` (python3 is already a build-time tool here, so no
  `zip` command is needed): a ZIP
  `pbnotes-inkpad-one-1.2.3.zip` holding `manifest.json` `{"version": "1.2.3"}` with a `VERSION` file `1.2.3` → expect pass for
  tag `v1.2.3`; fail for tag `v1.2.4`; fail when `VERSION` says `1.2.4`; fail when the ZIP is named `…-1.2.4.zip`; fail when the
  ZIP has no `manifest.json`; fail (exit 2) when the ZIP does not exist. Register
  `add_test(NAME check_release_selftest COMMAND ${CMAKE_SOURCE_DIR}/scripts/check-release.sh --self-test)` in
  `tests/CMakeLists.txt` beside `check_version_bump_selftest`. Observed failing (stub `check()` always passes)

### Implementation for User Story 4

- [x] T026 [US4] Implement `check()` in `scripts/check-release.sh` (R-7): arguments `[zip] [tag]` with the defaults of the
  contract; `VERSION` read like `package-device.sh` does; manifest version via `unzip -p "$zip" manifest.json | sed -n
  's/.*"version"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -n 1`; three comparisons, one `release check FAILED: …`
  line per mismatch on stderr, exit 1 for any, exit 2 for a missing ZIP; success prints `release check passed (<version>)`.
  `unzip` is the only tool beyond coreutils
- [x] T027 [US4] Run `ctest -R check_release_selftest --output-on-failure`: passes. Run `scripts/check-release.sh` by hand on
  a ZIP built in T010

**Checkpoint**: all four stories work; the release can be verified.

---

## Phase 7: Polish & cross-cutting

- [x] T028 [P] Edit `README.md`: (a) in the build prerequisites remove `zip (packaging)` and add
  "`pbstore-catalog` from the appstore-pocketbook repository (tag v0.1.1 or newer), on `PATH` or named by
  `PBSTORE_CATALOG`" with no real path; (b) after the device build command add the one-line explanation of what the script
  now does (manifest, pack twice, check); (c) in "Versions" replace "tag the release" by the release steps block of
  `contracts/packaging.md` (tag, `scripts/check-release.sh`, push, `gh release create` with notes = changelog entry + a link to
  `INSTALL.md` at that tag), with `scripts/check-release.sh` **chained** to the push and the release by `&&` so a mismatch stops
  the release (FR-013, US4 scenario 2); (d) mention `packaging/manifest.template.json` as the place to change manifest values;
  (e) change the README's package command from `build-arm/dist` to the default output folder `dist` (the folder
  `check-release.sh`, the release block and `quickstart.md` use), so the documented commands fit together
- [x] T029 Raise `VERSION` to `1.10.1` and add the `## 1.10.1` entry to `CHANGELOG.md` (above 1.10.0): the release package now
  follows the PB Store package contract (a `manifest.json`, built with the store's tool, reproducibly); install instructions
  are no longer inside the package but in the repository and the release notes; "No change to the app, to notes, sync or the
  data" (the style of earlier entries)
- [x] T030 Run `scripts/check-version-bump.sh` and `scripts/check-constitution.sh` (R5: no absolute local path in the new
  files; R2: no hard-coded SDK path) and `scripts/pre-merge.sh`; all green, including `packaging_script` and
  `check_release_selftest`. Fix findings in the files that cause them
- [x] T031 [MANUAL] Rebuild the ARM binary (README device build; `VERSION` is 1.10.1 now), package it with
  `scripts/package-device.sh build-arm/app/pbnotes.app "" dist`, and run `quickstart.md` §2 (all rows) and §3 (failure
  paths) with the real tool. Record the outputs (check result, SHA-256) in `research.md` under "Build check"
- [x] T032 [MANUAL] Quickstart §4: install the new package by hand on the InkPad One as `INSTALL.md` describes and start the
  app; record "works like 1.10.0" (or what differs) in `research.md` under "Device check". Mention the file-mode note of R-1
- [ ] T033 [MANUAL] After the merge to `main` and only when the maintainer says so: quickstart §5 — tag `v1.10.1`,
  `scripts/check-release.sh`, push the tag, create the GitHub release with the ZIP and the notes, download the asset and run
  `check-package` on it. Outward-facing: do not do this unprompted

---

## Dependencies & Execution Order

- **Phase 1** (T001, T002) first; T001 and T002 are independent of each other.
- **Phase 2** (T003 → T004 → T005) blocks everything; T003 is independent of T004/T005 and may run in parallel with them.
- **US1** (T006–T010) needs Phase 2. **US2** (T011–T019) edits the same script and test file as US1, so it follows US1.
- **US3** tests/implementation (T020–T024) need US1 (a staged tree to inspect); T022 and T023 touch other files and may run in
  parallel with US2.
- **US4** (T025–T027) is independent of US1–US3 (own script and test) and may run in parallel with them once Phase 2 is done
  (T005 only for the shared `tests/CMakeLists.txt`; edit that file serially).
- **Polish**: T028 and T029 after US1–US4; T030 after both; T031–T033 last and in order.

### Parallel opportunities

- T001 ∥ T002 ∥ T003 (three different files)
- After US1: T022 ∥ T023 (README ∥ INSTALL.md) while US2 is worked on
- US4 (T025–T027) ∥ US2 (T011–T019), different files
- T028 ∥ T029

## Implementation Strategy

- **MVP = US1**: after T010 the package passes the store's checker; that alone makes PB Notes listable once the entry exists.
- **Increment 2**: US2 (a trustworthy build) and US3 (docs), then US4 (the tag gate).
- Ship 1.10.1 only after Phase 7. Each story is testable on its own with the commands in its *Independent Test*.
- Out of scope, do not touch: the store entry in `appstore-pocketbook`, PB Cloud, PB Office, and any app code.
