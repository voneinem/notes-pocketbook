# Implementation Plan: About Box (Version, Author, License, Third-Party Licenses)

**Branch**: `003-about-box` | **Date**: 2026-10-11 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/003-about-box/spec.md`

## Summary

Three read-only screens reached from Settings — **About** (name, version, author, copyright,
license), **License** (the app's license text) and **Third-party licenses** (a list, and a text
page per component) — built on the existing architecture (Qt-free core, thin Qt bridge, QML UI):

- **One source of truth** — a hand-edited manifest `licenses/manifest.txt` declares the app and
  every third-party component (name, purpose, license, holder, address, text files, and the
  repository/build evidence that proves it exists). The app reads it at run time; `NOTICE` stays
  the hand-written legal notice. A host **consistency test** compares manifest, `NOTICE`,
  `third_party/`, the CMake dependency names and the text files, and fails the build when they
  drift (FR-012, FR-013).
- **Core** — a small Qt-free module parses and validates the manifest, reads license texts with
  size and path rules, splits them into paragraphs, and maps the build's version string to what is
  shown ("unknown" for the `0.0.0` fallback).
- **Bridge and UI** — one `AboutBridge` exposes the data; three QML pages show it with the app's
  e-ink conventions (black on white, flat buttons, paged `ListView`, no animation).
- **Packaging** — `package-device.sh` installs the manifest and exactly the texts it names into
  `pbnotes-licenses/` (same relative layout as the repository, so no path mapping), and checks that
  the packaged binary carries the version it is named after (SC-004). `NOTICE` gains the system
  components that were not in it.

Research resolved every unknown (R-1 to R-10); three small facts are **[VERIFY]** items with a
stated fallback, none blocks the design.

## Technical Context

**Language/Version**: C++17 (core, bridge, app); QML (Qt Quick 6.8); POSIX `sh` (packaging) —
unchanged.

**Primary Dependencies**: unchanged. No new library, no new third-party asset; license *texts* are
added as data files (provenance in `licenses/SOURCES.md`).

**Storage**: none written. Read-only files next to the app: `pbnotes-licenses/` on the device
(`/mnt/ext1/applications/pbnotes-licenses/`), the repository root on the host. About a dozen text
files, about 130 KiB in total.

**Testing**: the existing harness (host unit/integration tests with CTest, offscreen Qt for the
bridge, the app `--self-test`, manual quickstart on the device). New tests are listed in
research R-10 and quickstart §1.

**Target Platform**: PocketBook InkPad One (Qt 6.8.x runtime) and Linux x86-64 host — unchanged.

**Project Type**: desktop-style single application with a host-testable core library — unchanged.

**Performance Goals**: About opens in under 1 s; the list of 10 entries and any text page of up to
256 KiB appear in under 2 s on the device (SC-001 allows 10 s end to end); nothing is loaded until
the reader opens the page that needs it (manifest on opening About, a text on opening its page).

**Constraints**: offline; no network, no writes, no data sent anywhere (FR-005, FR-017, SC-005);
manifest ≤ 64 KiB and each text ≤ 256 KiB; manifest paths relative with no `..`; a missing or bad
item degrades to "unavailable" for that item only (FR-016); logs carry event names and counts only.

**Scale/Scope**: 11 manifest entries (app + 10 components), 3 new pages, 1 new bridge class, 1 new
core module (about 250 lines), 1 consistency test.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate (constitution) | Pre-research | Post-design | Evidence |
|---------------------|--------------|-------------|----------|
| I. Test-First | PASS | PASS | Every core function is pure or file-fixture testable; `tasks.md` must order each test before its implementation; test list in research R-10 and quickstart §1; the consistency test has negative cases proving each rule fires |
| II. Simplicity (YAGNI) | PASS with justified items | PASS with justified items | Three items in Complexity Tracking (manifest + parser, mirrored text tree, consistency test). Rejected as YAGNI: online links, a changelog, an update check, a license-text viewer with search, generating `NOTICE`, listing the device's whole library closure |
| III. Security & Privacy | PASS | PASS | No network use, no user data read or written; the manifest and texts are treated as external input (size caps, relative paths only, no `..`, resolved below the licenses root, unknown keys ignored, bad blocks isolated); logs carry counts only and the existing rule R6 forbids path/file/name log keys; no secret in the repository |
| IV. Integration tests at device boundaries | PASS | PASS | Contract test for the manifest format against the real `licenses/` tree (`test_about_files`); the file-layout boundary (installed tree on the reader) is covered by the packaging check and quickstart §3 |
| V. Host-testable core | PASS | PASS | Manifest, text and version logic live in `notes_core` with no Qt/InkView; only `AboutBridge` and the pages use Qt |
| VI. No silent data loss | PASS | PASS | Read-only feature; opening or leaving the pages changes no note, setting or sync state (tested: SC-005) |
| Stack: C++17, Qt 6.8 Quick/QML, CMake, libcurl, SQLite | PASS | PASS | No stack change |
| SDK location only from `POCKETBOOK_SDK` / `-DSDK_PATH` | PASS | PASS | No build path added; the packaging script reads files from the repository only; the research notes cite SDK facts without paths |
| Both device and host build targets | PASS | PASS | New core files build in both; the licenses root is a compile-time definition with an environment override, like the emoji font |
| Licensing: InkFeed attributed; notes-android reference-only; MIT project | PASS | PASS | `NOTICE` is extended to every manifest component (the consistency test enforces it); nothing is taken from notes-android; the license texts are the standard published texts, with provenance recorded |

No violations block planning.

## Project Structure

### Documentation (this feature)

```text
specs/003-about-box/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── manifest-format.md   # licenses/manifest.txt grammar, limits, consistency rules
│   ├── core-library.md      # New Qt-free interfaces
│   └── qml-bridge.md        # AboutBridge, the three pages, Settings change
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root) — additions and changes only

```text
licenses/                         # NEW  the single source of truth for the screens
├── manifest.txt                  # NEW  app + 10 components (contracts/manifest-format.md)
├── SOURCES.md                    # NEW  where each license text was taken from
├── inkfeed-MIT.txt, MPL-2.0.txt, Apache-2.0.txt, LGPL-2.1.txt, LGPL-3.0.txt,
│   GPL-3.0.txt, curl.txt         # NEW  standard texts (plain UTF-8)
core/include/pbnotes/
└── about.h                       # NEW  AppInfo, Component, Manifest, parse_manifest,
                                  #      load_text, split_paragraphs, display_version
core/src/
└── about.cpp                     # NEW
bridge/
├── include/about_bridge.h        # NEW  AboutBridge (QObject)
└── src/about_bridge.cpp          # NEW
app/
└── main.cpp                      # CHG  register the `about` context property; licenses root
qml/
├── pages/AboutPage.qml           # NEW  name, version, author, copyright, license, two buttons
├── pages/ThirdPartyPage.qml      # NEW  paged list of components
├── pages/LicenseTextPage.qml     # NEW  header block + paged paragraphs (app license and detail)
├── pages/SettingsPage.qml        # CHG  "About" button
└── qml.qrc                       # CHG
CMakeLists.txt                    # CHG  strip leading "v" from the version; PBNOTES_LICENSES_ROOT
scripts/package-device.sh         # CHG  install pbnotes-licenses/ from the manifest; version guard
NOTICE                            # CHG  add Qt, libcurl, OpenSSL, InkView, glibc, GCC runtime
README.md, INSTALL.md             # CHG  mention the About screen and the packaged licenses
tests/
├── unit/                         # test_about_manifest, test_about_text, test_about_version,
│                                 # test_license_consistency
├── integration/                  # test_about_files (real manifest + texts)
├── bridge/                       # test_about_bridge
└── support/license_check.h       # NEW  consistency rules as a function (tested with negative cases)
```

**Structure Decision**: no new top-level project; one new data directory (`licenses/`), one core
module, one bridge class, three pages. The manifest and the texts live outside `third_party/`
because they describe components that are not vendored (Qt, curl, glibc) as well as those that
are.

## Spec amendments made during planning

1. Assumptions: the set of "third-party components" is now defined by a rule (R-1: compiled in or
   shipped, named as needed by the binary, or performing a function the app relies on) and listed
   in research R-2. The spec's own wording (FR-008) is unchanged.
2. Assumptions: the version shown is the build's version identifier; until a release is tagged this
   is a commit identifier (R-6).

## Risks

| # | Risk | Effect | Mitigation |
|---|------|--------|------------|
| 1 | InkView's terms cannot be found | The entry cannot name a license | Entry states only the known facts (library of the reader's firmware, not redistributed here) with the holder and an address; no license name is invented (R-2) |
| 2 | A standard text is missing on the maintainer's machine | Packaging fails | The texts are committed in `licenses/`; only the one-time collection needs the system copies |
| 3 | Long texts render slowly on the device | Slow page | Paragraph `ListView` (R-7); load text only when its page opens; size cap 256 KiB |
| 4 | Rotation loses the reader's place | FR-015 not met | Remember the top paragraph index and reposition on size change; checked in quickstart §3 |
| 5 | The CMake dependency scan misses a new kind of dependency | A component slips in unlisted | The scan is one layer; `third_party/` and `NOTICE` checks and review remain; unknown kinds are added to the scan when found |
| 6 | The device's Qt, curl or glibc license differs from what the SDK headers state | A wrong license name on screen | Names come from the SDK's own headers (R-2); release check re-reads them when the SDK version changes |
| 7 | `LICENSE` copyright line changes without the manifest | Two different copyright lines | Consistency test compares the first `Copyright` line (R-3) |

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| Manifest file plus parser in the core | One declared list drives the screens and is checked against `NOTICE`, `third_party/` and CMake (FR-012, FR-013) | Hard-coding the list in QML cannot be tested against `NOTICE`; parsing `NOTICE` cannot carry purpose or text paths; a generator for 11 entries adds a tool and a generated file |
| Mirrored text tree on the device (`pbnotes-licenses/`) | Same relative paths on host and device, so no path mapping and no duplicated `LICENSE`; packaging copies exactly what the manifest names | A flat folder needs a name map and a collision rule; embedding in the binary hides the texts from the consistency test and needs a rebuild for a text fix |
| Consistency test with negative cases | The only mechanism that makes "adding a component without listing it fails before release" true | Review alone, which the spec (FR-013) explicitly says is not enough |
