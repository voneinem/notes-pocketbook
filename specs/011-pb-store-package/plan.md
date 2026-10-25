# Implementation Plan: PB Store Release Package

**Branch**: `feature/011-pb-store-package` | **Date**: 2026-10-24 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/011-pb-store-package/spec.md`

## Summary

The release ZIP gets a `manifest.json` at its root, loses `INSTALL.md` from its root, and is assembled by the store's own
tool `pbstore-catalog pack`, so that `pbstore-catalog check-package` passes (today it reports exactly E-PKG-04 and
E-PKG-05). It is a **packaging change only**: no app code, no QML, no core, no schema. The files under `applications/` are
the ones `scripts/package-device.sh` already installs. It ships in one slice:

1. **Manifest template** `packaging/manifest.template.json` with every fixed value and one placeholder, `@VERSION@`.
2. **`scripts/package-device.sh`** stages the same `applications/` tree, writes the manifest (version from the same
   variable that names the ZIP and is checked against the binary), finds the tool (`PBSTORE_CATALOG`, else `PATH`; clear
   failure otherwise), packs twice and requires byte-identical results, runs `check-package` on the result, and only then
   puts the ZIP in the output folder. `INSTALL.md` is no longer an input or an entry.
3. **`scripts/check-release.sh`** compares tag, `VERSION`, ZIP name and the manifest inside the ZIP (FR-013) and has a
   `--self-test` like the other gates. The tag, the GitHub release and the notes stay a deliberate step of the
   maintainer, documented in the README.
4. **Docs**: README (build, tool, release steps chained with `&&`, package command writing to `dist`; `zip` no longer a
   prerequisite), `INSTALL.md` (one sentence: the package also holds a `manifest.json` for the store; the README no longer
   says the file is part of the package), `CHANGELOG.md`, `NOTICE` untouched.
5. **Release** 1.10.1 (PATCH: the installed app is byte-for-byte the 1.10.0 app apart from its version string).

The resolved unknowns are in [research.md](research.md) (R-1 … R-8); the interfaces are in
[contracts/packaging.md](contracts/packaging.md); the file/entity description is in [data-model.md](data-model.md).
A trial on the real 1.10.0 files (R-1) already showed that a staged tree with this manifest packs, packs
reproducibly and passes `check-package`.

## Technical Context

**Language/Version**: Bash (existing scripts are `#!/usr/bin/env bash`, `set -euo pipefail`); JSON for the template. No C++/QML.

**Primary Dependencies**: `pbstore-catalog` from `appstore-pocketbook` (tag v0.1.1 or newer; contract schema 1.0) as an
external, maintainer-installed build tool — not vendored, not a library dependency of the app. `unzip` (read the manifest
back out of the ZIP in `check-release.sh`), `cmp`, `sed`, `mktemp` (coreutils). `zip` is no longer used.

**Storage**: none. Files only: template in `packaging/`, staging in a temp dir, output in `dist/` (git-ignored).

**Testing**: shell tests registered in CTest like `check_version_bump_selftest` / `launcher_script`:
`tests/packaging/test_package_device.sh` (drives the script with a fake ARM "binary" and **stub** `pbstore-catalog`
executables for the failure paths; runs the real tool for the happy path when it is available, else reports SKIP) and
`scripts/check-release.sh --self-test`. The device/real-tool walk-through is `quickstart.md`.

**Target Platform**: Linux x86-64 maintainer machine (the ARM binary is cross-built as before); the package targets the
InkPad One, firmware ≥ 6.11.1437.

**Project Type**: single application with a host-testable core — unchanged; this feature touches only build/release scripts.

**Performance Goals**: build overhead of two packs + one check on a 2.6 MB package: a few seconds (R-1: well under 5 s
here). SC-005: a missing tool is reported before any staging work.

**Constraints**: no machine-specific path to the tool in the repo (constitution R5/R2 in spirit and `check-constitution.sh`
R5 literally: no absolute local paths in tracked text files); `applications/` content and the ZIP file name unchanged;
the build never leaves a ZIP that looks valid after a failure; nothing in the app changes.

**Scale/Scope**: 1 new JSON template, 1 script edited (~40 lines net), 1 script added (~70 lines with self-test),
1 test script, 3 doc files, VERSION/CHANGELOG.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate (constitution) | Pre-research | Post-design | Evidence |
|---------------------|--------------|-------------|----------|
| I. Test-First | PASS | PASS | Each behavior is a script outcome that a shell test can observe: manifest content, top-level entries, tool lookup order, missing tool, non-reproducible pack, failing check, tag mismatch. `tasks.md` orders `tests/packaging/test_package_device.sh` and the `check-release.sh` self-test (observed failing against today's script) before the script changes |
| II. Simplicity (YAGNI) | PASS | PASS | No new language or framework; one template, one env var, one new 70-line gate. Rejected: generating JSON with a JSON tool (`sed` on one placeholder is enough, R-3), a Makefile/CMake packaging target, a release-automation workflow (publishing stays manual, spec assumption), vendoring or downloading `pbstore-catalog`, checking the tool's version, a `--remote` download check in the script (quickstart does it by hand) |
| III. Security & Privacy | PASS | PASS | No network access added (`pack` and `check-package` are offline, per the CLI contract). No secret, no user data. The tool is run from `PATH` or an explicit variable the maintainer controls; nothing is downloaded or executed from the network |
| IV. Integration tests at device boundaries | PASS | PASS | The ZIP/manifest is a boundary format of the store: the happy-path test runs the store's own `check-package` on a package built from fixture inputs (a contract test by the store's checker), and `check-release.sh` reads the manifest back from the finished ZIP |
| V. Host-testable core | PASS | PASS | `core/` is not touched; the tests run on the host without the SDK (fake binary containing the version string) |
| VI. No silent data loss | PASS | PASS | No user data is touched. The package still excludes `system/pbnotes/` (the user's database), and `data_dir` tells the store to keep it on removal |
| Stack: C++17, Qt 6.8, CMake, libcurl, SQLite | PASS | PASS | No stack change; build tooling only |
| SDK location only from env / `-DSDK_PATH` | PASS | PASS | Same rule applied to the new tool: `PBSTORE_CATALOG` or `PATH`, no default path, no path in docs examples (placeholders only) |
| Both device and host build targets | PASS | PASS | Unchanged; the host target runs the new packaging tests |
| Licensing: InkFeed attributed; notes-android reference-only | PASS | PASS | `package-device.sh` is already attributed in `NOTICE`; the change is our own. Nothing from notes-android. `pbstore-catalog` (same author, MIT) is run, not copied |
| Release version | PASS | PASS | Last phase of `tasks.md`: `VERSION` 1.10.1 + `CHANGELOG.md` entry, enforced by `check-version-bump.sh`; tag `v1.10.1` after the merge, verified by `check-release.sh` |

No Complexity Tracking entry is needed.

## Project Structure

### Documentation (this feature)

```text
specs/011-pb-store-package/
├── plan.md              # This file
├── research.md          # Phase 0: R-1 … R-8
├── data-model.md        # Phase 1: package, manifest, template, release
├── quickstart.md        # Phase 1: build, check, tag, publish, verify
├── contracts/
│   └── packaging.md     # script interfaces, template, error messages
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks - not created here)
```

### Source Code (repository root)

```text
packaging/
└── manifest.template.json        # NEW   fixed manifest values + @VERSION@

scripts/
├── package-device.sh             # CHG   stage manifest, find tool, pack twice + cmp, check-package, publish ZIP;
│                                 #       INSTALL.md no longer an input or entry; header comment updated
└── check-release.sh              # NEW   tag == VERSION == ZIP name == manifest version (+ --self-test)

tests/
├── CMakeLists.txt                # CHG   register packaging_script and check_release_selftest
└── packaging/
    └── test_package_device.sh    # NEW   fake binary + stub/real pbstore-catalog; failure paths

README.md                         # CHG   build + tool + release steps; INSTALL.md link; drop `zip` prerequisite
INSTALL.md                        # CHG   one sentence about manifest.json; otherwise unchanged (manual route)
CHANGELOG.md, VERSION             # CHG   1.10.1
```

**Structure Decision**: keep the existing single-script packaging (`scripts/package-device.sh`) and add the template under a
new top-level `packaging/` folder, the location the store's adoption guide suggests. The tag check is its own small script
because it runs at release time, after the ZIP exists, not at build time, and it follows the pattern of the other
`scripts/check-*.sh` gates (including `--self-test`).

## Complexity Tracking

No constitution violation; nothing to justify.
