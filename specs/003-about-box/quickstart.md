# Quickstart: validating the About Box

Validation guide only; implementation details live in `tasks.md`. Contracts:
[manifest-format](contracts/manifest-format.md), [core-library](contracts/core-library.md),
[qml-bridge](contracts/qml-bridge.md). Data: [data-model](data-model.md). Decisions:
[research](research.md). Build, packaging and device prerequisites are those of
`specs/001-pocketbook-notes-mvp/quickstart.md` §1–§3.

## 1. Host build and tests (no device)

```bash
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host
ctest --test-dir build-host --output-on-failure
scripts/pre-merge.sh            # tests, constitution checks, fixture anonymization, secret scan
```

Expected: all suites pass offline. New tests (intended names): `unit/test_about_manifest`,
`unit/test_about_text`, `unit/test_about_version`, `unit/test_license_consistency`,
`integration/test_about_files`, `bridge/test_about_bridge`. Each is written first and observed
failing before its implementation (constitution I). The app `--self-test` (already in CTest as
`app_self_test`) must still pass with the three new pages.

### 1a. Prove the drift checks fire (FR-013)

Each of these must make `ctest -R test_license_consistency` fail, naming the rule and the entry;
undo each change afterwards.

| Change | Expected finding |
|--------|------------------|
| Add a directory `third_party/dummy/` | rule 2: directory not claimed by any entry |
| Add `find_package(ZLIB REQUIRED)` to a CMake file | rule 3: package `ZLIB` not claimed |
| Rename an entry's `name` in the manifest but not in `NOTICE` | rule 1: no `NOTICE` section for the entry |
| Add a section to `NOTICE` with no manifest entry | rule 1: section without entry |
| Point an entry's `text:` at a missing file | rule 4: file missing |
| Change the copyright year in the manifest only | rule 5: differs from `LICENSE` |

The negative cases also run on in-memory data inside the test, so the suite proves the rules work
without anyone editing files.

## 2. Packaging check

```bash
cmake -S . -B build-arm -G Ninja -DTARGET_TYPE=ARM -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm
scripts/package-device.sh build-arm/app/pbnotes.app "" build-arm/dist
unzip -l build-arm/dist/pbnotes-inkpad-one-*.zip | grep pbnotes-licenses
```

Expected: `pbnotes-licenses/licenses/manifest.txt`, `pbnotes-licenses/LICENSE`,
`pbnotes-licenses/third_party/noto-emoji/OFL.txt` and every `licenses/*.txt` named in the manifest;
the zip name's version equals the version string inside the binary (the script fails otherwise,
SC-004). Remove a text file from `licenses/` and rerun: packaging must fail naming the file.

## 3. On the reader (InkPad One)

Install as in `INSTALL.md` (pre-authorised install path from the project memory applies to the
pbnotes files only). Switch off Wi-Fi before starting to prove the offline claims.

| Step | Expected |
|------|----------|
| Notes list → Settings → About | About shows app name, version equal to the zip's version, author "Volker von Einem", the copyright line, "License: MIT License" (≤ 2 taps, SC-001) |
| Compare with `LICENSE` | copyright line identical |
| License | Full MIT text with the copyright line; Page down reaches the end; Back returns to About |
| Third-party licenses | List of 10 rows, first those "In the app package" (InkFeed, Mozilla CA bundle, SQLite, Noto Emoji), then those "Provided by the reader" (Qt, libcurl, OpenSSL, PocketBook InkView, GNU C Library, GCC runtime libraries); each row has name, purpose, license name and origin |
| Open each entry | Holder, license, address and full text; for SQLite and InkView the address line instead of a text (FR-010); long texts (GPL-3.0) page to the last line; Back returns to the list (≤ 2 taps from About, SC-003) |
| Rotate the reader while a license text is open, then back | Same page, same place; layout fits the new orientation (FR-015) |
| Hold a page button on the list | Pages move by whole rows; every entry is reachable |
| Wi-Fi off | Everything above works; nothing tried to connect (log shows no network events from these pages) |
| Sync before and after visiting About | Note counts and sync status unchanged (SC-005) |

### 3a. Degraded installs

1. Rename `pbnotes-licenses/licenses/MPL-2.0.txt` on the reader and open the Mozilla CA entry:
   header fields shown, text replaced by "The license text could not be read."; other entries fine.
2. Rename `pbnotes-licenses/licenses/manifest.txt`: About shows name and version, the other items
   "unavailable", the list shows "N entries could not be read."; no crash, Back works (FR-016, SC-006).
3. Restore the files.

## 4. Release check (SC-002, SC-004)

Before tagging a release: `ctest` green (includes the consistency test), `NOTICE` reviewed against
the SDK's current library versions, the About version equals the release identifier, and the three
**[VERIFY]** facts in `research.md` R-2 are settled (InkView terms, GCC runtime exception text,
curl license year range).
