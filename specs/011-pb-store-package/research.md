# Research: PB Store Release Package

Each entry: decision, rationale, alternatives. "Trial" means a run made while planning, on the real 1.10.0 package contents
with the sibling `appstore-pocketbook` checkout at tag v0.1.1 (`pbstore-catalog 0.1.0`, schema 1.x).

## R-1 What the store's tool does with our files (trial)

**Decision**: stage `applications/` + `manifest.json` in a directory and call `pbstore-catalog pack <dir> --out <zip>`.

**Evidence**: the 1.10.0 ZIP unpacked, `INSTALL.md` removed, the template manifest (with `version` 1.10.0) added:
- `pack` exit 0, 18 entries, prints size, SHA-256 and `app: pbnotes 1.10.0`.
- Packing the same directory twice gave byte-identical ZIPs (same SHA-256).
- `check-package` on the result: exit 0 ("package ok").
- `check-package` on the shipped 1.10.0 ZIP: exit 1 with exactly `E-PKG-04 INSTALL.md` and `E-PKG-05 manifest.json`.
- `pack` on a missing directory: exit 2 ("package directory not found"), writes nothing.
- The packed ZIP has no directory entries and every file has mode 0644 (the old ZIP carried 0777 on `pbnotes.bin` and
  `pbnotes.app`). The reader's storage is a FAT-style volume copied over USB, where modes do not exist, and the manual route
  of INSTALL.md never relied on them; the store installer is out of scope. Noted, not worked around.

**Alternatives**: keep `zip` and add the manifest by hand (rejected: spec requires the tool; the file list with SHA-256 would
have to be computed by us).

## R-2 Finding the tool: `PBSTORE_CATALOG`, else `PATH`

**Decision**: if the environment variable `PBSTORE_CATALOG` is set, it must name an executable file (the tool itself); if it
is unset, `command -v pbstore-catalog`. A set-but-invalid variable is an error (it does not silently fall back to `PATH`).
No default path. The same resolved command runs `pack` and `check-package`.

**Missing-tool message** (stderr, exit 1, before any staging):
`pbstore-catalog not found. Put it on PATH or set PBSTORE_CATALOG to the tool (build it from the appstore-pocketbook repository, tag v0.1.1 or newer).`

**Rationale**: mirrors the project's own rule for the SDK (`POCKETBOOK_SDK` only from the environment). A variable that is
set wrongly is a configuration error the maintainer wants to hear about.

**Alternatives**: a `--tool` argument (rejected: the script already takes positional arguments used by the README and
earlier docs; an environment variable keeps them unchanged); a default sibling-directory guess (rejected: a hidden path,
and constitution R5 forbids absolute paths in tracked files).

## R-3 Writing the manifest: one placeholder, `sed`

**Decision**: `packaging/manifest.template.json` holds the fixed values and `"version": "@VERSION@"`. The script runs
`sed "s/@VERSION@/$VERSION/g"` into the staging folder, after checking `$VERSION` matches `^[0-9]+\.[0-9]+\.[0-9]+$`
(the same rule as `check-version-bump.sh`, so the value needs no JSON escaping). It then fails if any `@[A-Z_]+@` remains.
`files` is not in the template: `pack` computes it.

**Rationale**: the docs of the store show exactly this shape; the version is validated first so `sed` is safe; the
leftover-placeholder check covers the "template changed, script not" edge case.

**Alternatives**: `jq` (rejected: a new tool dependency for one string); generating the JSON in the script (rejected: the
spec wants a template in the repository).

## R-4 Reproducibility check

**Decision**: pack twice into two temporary files in the temp dir, `cmp` them; on a difference fail with
`package is not reproducible: two builds from the same files differ`. The first file is then checked and published.

**Rationale**: `pack` is documented and observed (R-1) to be deterministic, so the second pack costs about a second and
proves it for every build instead of trusting it; it would also catch a future tool change.

**Alternatives**: trust the documentation (rejected by FR-006); pack twice from two separate staging copies (rejected: the
only way the two could differ is the staging tree itself, which is one directory — a second copy tests nothing more).

## R-5 Verifying and publishing atomically

**Decision**: order inside the script: validate inputs → resolve tool → stage → manifest → remove any existing target ZIP →
pack ×2 → `cmp` → `check-package` on the first → `mv` it to `dist/pbnotes-inkpad-one-<version>.zip` → print the path (as
today) and the tool's size/SHA-256 lines. Any failure exits non-zero before the `mv`, so the output folder never contains
a new, unchecked ZIP; the old ZIP of the same version is removed first so it cannot be mistaken for the result.

**Rationale**: matches the spec's edge cases (replace, never merge; no valid-looking ZIP after a failure).

## R-6 Where the install instructions go (decided in the clarification)

**Decision**: they are not shipped. `INSTALL.md` stays in the repository, the README links it, and the GitHub release notes
carry a tag-pinned link to it (`.../blob/v<version>/INSTALL.md`) next to the changelog entry. `INSTALL.md`'s statement
"(also part of the package)" in the README is removed; its own text (copy `applications/`, register `U_PbNotes`) is still
right for the manual route because the `applications/` set is unchanged.

**Alternatives**: copy under `applications/` (rejected by the user's choice B: keeps the installed file set identical).

## R-7 Tag/version check as a small gate script

**Decision**: `scripts/check-release.sh [<zip>] [<tag>]` (defaults: `dist/pbnotes-inkpad-one-$(cat VERSION).zip`,
`v$(cat VERSION)`). It reads `manifest.json` from the ZIP with `unzip -p`, extracts `"version"` with `sed`, and requires:
tag = `v` + manifest version, manifest version = `VERSION`, ZIP file name = `pbnotes-inkpad-one-<manifest version>.zip`.
Exit 1 with one line per mismatch. `--self-test` builds throw-away ZIPs (with `python3 -m zipfile`, so no `zip` command is
needed) to show pass and each mismatch fail. Not part of `pre-merge.sh` (no ZIP exists there); it is a step of
the documented release procedure and of `quickstart.md`.

**Rationale**: the store checks `E-ENT-14` (tag in `package_url` = manifest version) only when the entry is added, a repo
away; failing locally before the release is published is cheaper. A tag mismatch cannot be caught by `check-package`.

**Alternatives**: do it inside `package-device.sh` (rejected: the tag may not exist at build time and the build must not
depend on git state); a CI workflow (rejected: CI is deferred in this project, see `pre-merge.sh`).

## R-8 Version number of this change: 1.10.1

**Decision**: PATCH → `VERSION` 1.10.1, tag `v1.10.1`.

**Rationale**: the already-published `v1.10.0` has the non-conforming ZIP and a tag must not move; a conforming package
needs a new version. The installed app does not change; the package does (a fix). The ARM binary must be rebuilt because it
carries the version string (the packaging script refuses a mismatch). `scripts/check-version-bump.sh` requires the bump,
since `scripts/` and `packaging/` count as product files.

**Alternatives**: MINOR 1.11.0 (rejected: no feature for the reader); re-tag 1.10.0 (rejected: moving a published tag breaks
anyone who has it).

## Out of scope, noted for later

- The catalog entry (`catalog/entries/pbnotes.json`) in `appstore-pocketbook`; its `package_url` must use the tag chosen here.
- `min_firmware` is the author's firmware by decision; other models/firmware are untested and not claimed.

## Baseline (1.10.0)

`pbstore-catalog check-package dist/pbnotes-inkpad-one-1.10.0.zip` (tool 0.1.0), exit 1:

```text
error[E-PKG-04] INSTALL.md: "INSTALL.md" is not allowed at the package root (only manifest.json and the applications/ tree)
error[E-PKG-05] manifest.json: manifest.json is missing from the package root
```

The ZIP holds `INSTALL.md`, 17 files under `applications/` (listed in `tests/packaging/applications.expected`, sorted with
`LC_ALL=C sort`) and the directory entries `applications/…/`.

## Build check (1.10.1, quickstart §2 and §3)

Date 2026-10-24. ARM build (`cmake --build build-arm`, ELF 32-bit ARM EABI5, contains `1.10.1`), packaged with
`scripts/package-device.sh build-arm/app/pbnotes.app "" dist`, `pbstore-catalog 0.1.0` via `PBSTORE_CATALOG`.

| Check | Result |
|-------|--------|
| `check-package dist/pbnotes-inkpad-one-1.10.1.zip` | exit 0, `package ok`, 2638866 bytes, SHA-256 `610c52f8fc51ad8dc0454faddc08edc9b21d20fe495cabaa0ca35e27d9a7eb1f` |
| Top level | only `manifest.json` (plus `applications/`); no `INSTALL` entry |
| Manifest | all values of `data-model.md`, `version` 1.10.1, 17 files listed |
| `applications/` vs the 1.10.0 ZIP | identical file lists; `applications/pbnotes.bin` equals the built binary byte for byte |
| Reproducible | a second build one second later: same SHA-256 |
| `scripts/check-release.sh … v1.10.1` | `release check passed (1.10.1)`; with `v0.0.0`: exit 1, `tag v0.0.0 does not match manifest version 1.10.1` |
| Tool not on `PATH`, variable unset | exit 1, `pbstore-catalog not found. Put it on PATH or set PBSTORE_CATALOG …`, output folder not created |
| `PBSTORE_CATALOG=<nonexistent>` | exit 1, `PBSTORE_CATALOG is set but is not an executable file` |
| Version argument 9.9.9, binary built for 1.10.1 | exit 1, `version mismatch`, no ZIP |

The ZIP is 2638866 bytes, the 1.10.0 ZIP 2640388 bytes (no `INSTALL.md`, no directory entries, a manifest with the file
list added). The `placeholder` row of §3 is covered by `packaging_script`.

## Device check (1.10.1, quickstart §4)

Date 2026-10-25. InkPad One, firmware 6.11.1437, reader volume mounted over USB (vfat). The files of
`applications/` from `dist/pbnotes-inkpad-one-1.10.1.zip` were compared with the installed 1.10.0 set: only
`applications/pbnotes.bin` differed, so only it was copied (verified with `cmp`); the launcher script, icons, CA bundle,
emoji font and license texts were already identical. `view.json` and the notes database were not touched.

| Check | Result |
|-------|--------|
| App starts from the PB Notes menu entry | works, same as 1.10.0 |
| Launch log (`system/pbnotes/pbnotes.log`) | `Starting task - pbnotes.app`, `pbnotes.bin` started from `/mnt/ext1/applications/`, theme opened, no error |
| Mode change by `pack` (0644 instead of 0777 on `pbnotes.bin` / `pbnotes.app`) | no effect: the reader's FAT volume has no file modes |

Result: the manual-install route (US3 scenario 2, FR-011, SC-003) works with the package built by `pbstore-catalog pack`.
