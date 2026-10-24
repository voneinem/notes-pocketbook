# Quickstart: PB Store Release Package

Validation guide for feature 011. Interfaces: [contracts/packaging.md](contracts/packaging.md). Values:
[data-model.md](data-model.md).

## 1. Host checks (no SDK, no device)

```bash
cmake -S . -B build-host -G Ninja && cmake --build build-host
ctest --test-dir build-host -R "packaging_script|check_release_selftest|check_version_bump_selftest" --output-on-failure
```

Expected: all pass. `packaging_script` runs the failure paths with stub tools; its happy-path part runs the real
`pbstore-catalog` when `PBSTORE_CATALOG` or `PATH` provides it and reports a skip otherwise.

## 2. Build the package (needs the ARM build, see README)

Prerequisite: `pbstore-catalog` from `appstore-pocketbook` (tag v0.1.1 or newer) on `PATH`, or
`export PBSTORE_CATALOG=<path to the tool>`.

```bash
scripts/package-device.sh build-arm/app/pbnotes.app "" dist
```

Expected: the path `dist/pbnotes-inkpad-one-<version>.zip` on stdout; on stderr the tool's `package ok`-style report
(size, SHA-256, `app: pbnotes <version>`).

| Check | Command | Expected |
|-------|---------|----------|
| Store contract (SC-001) | `pbstore-catalog check-package dist/pbnotes-inkpad-one-<version>.zip` | exit 0, no `E-PKG-*` |
| Top level (FR-001) | `unzip -Z1 dist/*.zip \| grep -v '^applications/'` | only `manifest.json` |
| Manifest values (FR-003) | `unzip -p dist/*.zip manifest.json` | the values of the data model; `version` = `cat VERSION` |
| Same files as 1.10.0 (SC-003) | compare `unzip -Z1` of both ZIPs, minus `manifest.json`, `INSTALL.md` and directory entries | identical lists |
| Reproducible (SC-002) | build twice, `sha256sum` both | identical |
| No INSTALL.md (FR-005) | `unzip -Z1 dist/*.zip \| grep -c INSTALL` | 0 |

## 3. Failure paths

| Setup | Expected |
|-------|----------|
| tool not on `PATH`, `PBSTORE_CATALOG` unset | exit 1, `pbstore-catalog not found. Put it on PATH or set PBSTORE_CATALOG …`, no ZIP (SC-005) |
| `PBSTORE_CATALOG=/nonexistent` | exit 1, `PBSTORE_CATALOG is set but is not an executable file` |
| `VERSION` changed, binary not rebuilt | exit 1, `version mismatch` (as before) |
| a `@X@` added to the template | exit 1, `unfilled placeholder` |

## 4. Manual install still works (device, once)

Unzip the new package onto the reader as `INSTALL.md` describes (copy `applications/`, register `U_PbNotes`, reboot) and
start **PB Notes**. Expected: the same as with 1.10.0. Record the result in `research.md` under "Device check".

## 5. Release

```bash
git tag "v$(cat VERSION)"                       # after the merge
scripts/check-release.sh                        # expected: release check passed (<version>)
git push origin "v$(cat VERSION)"
gh release create "v$(cat VERSION)" "dist/pbnotes-inkpad-one-$(cat VERSION).zip" --title "<version>" --notes-file <notes>
```

Then verify the published asset (US4, scenario 3):

```bash
gh release download "v$(cat VERSION)" --pattern '*.zip' --dir "$(mktemp -d)"   # then run check-package on it
```

Expected: `check-package` exit 0 on the downloaded file. Publishing is a deliberate maintainer step (the push and the
release are outward-facing); do it only when intended.
