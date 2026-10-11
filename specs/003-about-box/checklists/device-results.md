# Verification results: About Box

## Drift checks (quickstart §1a, task T043) — done 2026-10-11, host

Each edit was made, `test_license_consistency` run, and the edit reverted. All six were reported:

| Edit | Finding |
|------|---------|
| Add `third_party/dummy/` | `rule 2: third_party/dummy is not claimed by any entry` |
| Add `find_package(ZLIB REQUIRED)` to `core/CMakeLists.txt` | `rule 3: CMake dependency 'ZLIB' is not claimed by any entry` |
| Rename an entry (SQLite → SQLite3) in the manifest only | `rule 1: no NOTICE section for 'SQLite3'` and `rule 1: NOTICE section 'SQLite (public domain)' has no manifest entry` |
| Add a `NOTICE` section with no entry | `rule 1: NOTICE section 'Zlib (zlib license)' has no manifest entry` |
| Point an entry's `text:` at a missing file | `rule 4: 'libcurl': text file missing: licenses/nope.txt` |
| Change the copyright year in the manifest only | `rule 5: app copyright '...2027...' differs from LICENSE '...2026...'` |

## Packaging (quickstart §2, task T013/T014) — done 2026-10-11, host, fake binary

The zip contains `pbnotes-licenses/licenses/manifest.txt`, `LICENSE`, `third_party/noto-emoji/OFL.txt` and every
`licenses/*.txt` the manifest names. Packaging fails with a message when the binary lacks the version string, and
when a named text file is missing. Not yet run against a freshly built ARM binary.

## Build installed on the reader — 2026-10-11

ARM build from the working tree (version `0ae4a86-dirty`, uncommitted changes) packaged with
`scripts/package-device.sh` (the version guard passed against the real ELF) and copied to the reader:
`pbnotes.bin`, `pbnotes.app` and `pbnotes-licenses/`, all verified with `cmp`; the previous `pbnotes.log` was
moved to `pbnotes.log.run7`. Not yet launched or looked at.

## On the reader — OPEN (manual)

- [ ] T023 About screen (quickstart §3, first two rows)
- [ ] T031 License text, paging, rotation, offline (quickstart §3)
- [ ] T040 Third-party list and entries, degraded installs (quickstart §3, §3a)
- [ ] T044 Release check (quickstart §4)
