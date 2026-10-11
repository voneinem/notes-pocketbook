---

description: "Task list for the About Box (version, author, license, third-party licenses)"
---

# Tasks: About Box (Version, Author, License, Third-Party Licenses)

**Input**: Design documents from `/specs/003-about-box/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: REQUIRED. The constitution makes Test-First non-negotiable (Principle I): every test task
below MUST be written and observed FAILING before the implementation task(s) it names. Tests for
`notes_core` run on the host without Qt, the SDK or a device (Principle V). Unit and integration
tests under `tests/unit/` and `tests/integration/` are picked up by the glob in
`tests/CMakeLists.txt`; bridge tests must be added to the list in that file by hand.

**Organization**: Tasks are grouped by user story. Paths are relative to the repository root. This
feature builds on the finished MVP and feature 002; all existing tests must stay green.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US3 as in spec.md
- **[MANUAL]**: needs a person or the reader; record the result in the named file. An implementing
  agent skips these and reports them as open

## Conventions that apply to every task

- C++17; namespace `pbnotes` (the new module uses `pbnotes::about`); `notes_core` MUST NOT include Qt or
  `inkview.h` headers.
- TLS verification, SDK paths and logging rules are those of the constitution and are enforced by
  `scripts/check-constitution.sh`. In particular: SDK or toolchain paths MUST NOT appear in any
  committed file (use `$POCKETBOOK_SDK` when a task needs the SDK), and logs carry event names and
  counts only (no file path, file name, component name or license text).
- The feature is read-only: nothing here may write a file, open a network connection, or touch the
  store, the settings or the sync state (FR-005, FR-017).
- The manifest and the license texts are external input: apply the size caps and path rules of
  `contracts/manifest-format.md` in code, not by convention.
- UI strings go through `qsTr`; license texts and manifest strings are shown as read.

---

## Phase 1: Setup (license data)

**Purpose**: Collect the license texts the manifest will name. Pure data; no code.

- [X] T001 [P] Create `licenses/` and copy the standard texts unchanged (plain UTF-8, LF) from the
  maintainer's system copies: `licenses/MPL-2.0.txt`, `licenses/Apache-2.0.txt`,
  `licenses/LGPL-2.1.txt`, `licenses/LGPL-3.0.txt` (from `LGPL-3`), `licenses/GPL-3.0.txt` (from
  `GPL-3`). Source dir on Debian/Ubuntu: `/usr/share/common-licenses/`.
- [X] T002 [P] Create `licenses/curl.txt` from the curl license (`COPYING`) that accompanies the SDK's
  CMake sources; find it with `find "$POCKETBOOK_SDK" -path '*cmcurl/COPYING'` (never write the SDK
  path into a file). Keep the copyright line of that copy and note its year range for SOURCES.md.
- [X] T003 [P] Create `licenses/inkfeed-MIT.txt` holding the MIT text exactly as it appears in the
  InkFeed section of `NOTICE`, including `Copyright (c) 2026 Joost Rohde`.
- [X] T004 Settle the three **[VERIFY]** items of `research.md` R-2 and record each outcome there:
  (a) PocketBook InkView terms: look in the SDK tree and the SDK's public repository for a license
  statement; if none is found, the manifest entry names no license (see T012). (b) GCC Runtime
  Library Exception 3.1 text: look in the SDK tree; if found add `licenses/GCC-RLE-3.1.txt`,
  otherwise the entry gives the published address (https://www.gnu.org/licenses/gcc-exception-3.1.html).
  (c) The curl license year range from T002.
- [X] T005 Create `licenses/SOURCES.md`: one line per text file in `licenses/` saying where it was
  taken from (system package path pattern, SDK file name without path, or `NOTICE`), plus the
  outcomes of T004. Depends on T001–T004.

**Checkpoint**: `licenses/` holds every text the manifest will name.

---

## Phase 2: Foundational (blocking prerequisites)

**Purpose**: The manifest, its parser and loader, the version rule and the packaging of the tree.
Every user story reads the manifest.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

### Tests first (observe failing)

- [X] T006 [P] Write `tests/unit/test_about_manifest.cpp` for `pbnotes::about::parse_manifest` and
  `load_manifest` (`contracts/core-library.md`, rows of the behaviour table): well-formed manifest
  → `readable`, `app` set, components in file order, `bad_blocks == 0`; missing required key →
  block dropped and counted, rest parsed; duplicate `id` and second `kind: app` → later block
  dropped; `text:` values `../x`, `/etc/passwd`, `a\b`, a NUL and a value over 200 characters →
  block dropped; unknown key ignored; repeated key → last wins; CRLF and BOM; empty input →
  `readable`, no app, no components, `bad_blocks == 0`; input over 64 KiB → `readable == false`;
  more than 64 blocks → first 64 read; comment lines ignored; `load_manifest` on a missing file →
  `readable == false`. Field limits (quote verbatim in the test names): `id` `[a-z0-9-]+`, ≤ 40
  characters; `name` ≤ 80; `purpose` ≤ 120; `license` ≤ 120; `url` `https://…` ≤ 200; `origin` is
  `bundled` or `system`; `kind` is `app` or `component`. Use `tests/support/tmp_dir.h` for files.
- [X] T007 [P] Write `tests/integration/test_about_files.cpp`: load the REAL manifest from
  `PBNOTES_SOURCE_DIR` (repository root) with `load_manifest`; assert `readable`, `app` present,
  `bad_blocks == 0`, 10 components, every `text` path exists, is non-empty and ≤ 256 KiB, `origin` of
  every component set. This is the contract test for the manifest format (constitution IV). It
  fails until T012.

### Implementation

- [X] T008 Add `core/include/pbnotes/about.h` with the declarations of `contracts/core-library.md`
  needed so far (`Origin`, `Entry`, `Manifest`, `parse_manifest`, `load_manifest`) and an empty
  `core/src/about.cpp`; register `src/about.cpp` in `PBNOTES_CORE_SOURCES` in
  `core/CMakeLists.txt`. Build; run T006/T007 and observe them FAIL.
- [X] T009 Implement `parse_manifest` in `core/src/about.cpp` per `contracts/manifest-format.md`
  (grammar, keys, limits, bad-block rules, path rules). Make T006 pass except the `load_manifest`
  cases.
- [X] T010 Implement `load_manifest(licenses_root)` in `core/src/about.cpp`: reads
  `<root>/licenses/manifest.txt`, refuses files over 64 KiB, logs `about.bad_blocks` (count only)
  through `core/include/pbnotes/log.h`. Make T006 pass completely.
- [X] T011 [P] Make the build and packaging version one rule: in `CMakeLists.txt` strip one leading
  `v` from the `git describe --tags --always --dirty` result (keep the `0.0.0` fallback), so the
  compiled `PBNOTES_VERSION` and the string the packaging script derives are identical
  (research R-6).
- [X] T012 Create `licenses/manifest.txt` per `contracts/manifest-format.md` with the app block
  (`id: pbnotes`, `kind: app`, `name: Notes Pocketbook`, `license: MIT License`, `holder: Volker
  von Einem`, `copyright:` equal to the first `Copyright` line of `LICENSE`, `text: LICENSE`) and the
  10 component blocks of `research.md` R-2. Each component `name` MUST be a prefix of the heading
  line of its `NOTICE` section (existing headings: `InkFeed`, `Mozilla CA certificate bundle`,
  `SQLite`, `Noto Emoji`; new headings are written in T034 with the names `Qt 6.8 (Core, Gui, Qml,
  Quick)`, `libcurl`, `OpenSSL`, `PocketBook InkView`, `GNU C Library`, `GCC runtime libraries`).
  Set `origin`, `text`, `source` (`third_party/cacert`, `third_party/sqlite`,
  `third_party/noto-emoji`) and `cmake` (`Qt6`, `CURL`, `inkview`) as in research R-2. SQLite and
  InkView have no `text`. Use the T004 outcomes for InkView and the GCC runtime. `purpose` ≤ 120
  characters, `url` is `https://`. Make T007 pass.
- [X] T013 Update `scripts/package-device.sh`: install `licenses/manifest.txt` and every file named
  by any `text:` key of the manifest into `pbnotes-licenses/` of the stage with `install -D -m 644`,
  keeping the relative path; fail with a message naming the file when one is missing; do not install
  anything else from `licenses/`. Parse the `text:` lines with `awk`/`sed` (POSIX `sh`; comma lists).
  Keep the existing `pbnotes-emoji-OFL.txt`. Run it against a fake ELF and list the zip to check
  the tree (quickstart §2); also run it once with one text file temporarily removed and confirm it
  fails.
- [X] T014 In `scripts/package-device.sh`, derive the default version with the same rule as T011
  (`git describe --tags --always --dirty`, one leading `v` removed) and fail when the built ELF does
  not contain that version string (`grep -aF`), so the zip name and the About screen cannot differ
  (SC-004). Pass-through of an explicit version argument stays; the guard applies to it too.

**Checkpoint**: manifest parses, real tree validates, package contains the tree. User stories can start.

---

## Phase 3: User Story 1 - See which version of the app I have (Priority: P1) 🎯 MVP

**Goal**: Settings → About shows name, version, author, copyright and license name; Back returns.

**Independent Test**: Open Settings, choose About, check the values against the build and
`LICENSE`; Back shows Settings again. With the manifest missing the page still shows name and
version, the other items "unavailable".

### Tests first (observe failing)

- [X] T015 [P] [US1] Write `tests/unit/test_about_version.cpp`: `display_version("")` and
  `display_version("0.0.0")` → `"unknown"`; `"0ae4a86"`, `"0.2.0"`, `"0.2.0-3-gabc123-dirty"` →
  unchanged.
- [X] T016 [P] [US1] Write `tests/bridge/test_about_bridge.cpp` (offscreen Qt, fixture licenses root
  built in a `TmpDir`, style of `tests/bridge/test_attachments_bridge.cpp`) for the app part of
  `contracts/qml-bridge.md`: with a good manifest `appName`, `version` (via `display_version`),
  `author`, `copyright`, `licenseName`, `infoAvailable == true`; with no manifest and with a
  manifest lacking the app block `infoAvailable == false`, `author`/`copyright`/`licenseName`
  empty, `appName` and `version` still set, no crash; the data directory listing is identical before
  and after reading every property. Add `test_about_bridge` to the bridge test list in
  `tests/CMakeLists.txt`.

### Implementation

- [X] T017 [US1] Add `display_version` to `core/include/pbnotes/about.h` and `core/src/about.cpp`.
  Make T015 pass.
- [X] T018 [US1] Create `bridge/include/about_bridge.h` and `bridge/src/about_bridge.cpp`: `AboutBridge`
  (QObject) with `Config{licenses_root, version}` and the read-only properties `appName`, `version`,
  `author`, `copyright`, `licenseName`, `infoAvailable` (loads the manifest lazily on first read
  and caches it; never writes; logs `about.opened` once). Register the sources in
  `PBNOTES_BRIDGE_SOURCES` and the header in the `pbnotes_moc(...)` list (ARM) and in the host
  library of `bridge/CMakeLists.txt`. Make the app part of T016 pass.
- [X] T019 [US1] In `app/CMakeLists.txt` add the compile definition `PBNOTES_LICENSES_ROOT` (host:
  `${CMAKE_SOURCE_DIR}`); in `app/main.cpp` create an `AboutBridge` with the root
  `/mnt/ext1/applications/pbnotes-licenses` on the device (`PLATFORM_POCKETBOOK`), the compile-time
  root on the host, overridable by the environment variable `PBNOTES_LICENSES_ROOT`, and the
  version from `pbnotes::version()`; expose it as context property `about`.
- [X] T020 [P] [US1] Create `qml/pages/AboutPage.qml`: title "About"; app name (large), "Version
  <version>", "Author: <author>", the copyright line, "License: <licenseName>"; an empty value
  shows `qsTr("unavailable")`; a "Back" `InkButton` that pops the stack; black on white, no
  animation, `Flickable` with `PageButtons` when the content does not fit. Follow the structure of
  `qml/pages/SettingsPage.qml`.
- [X] T021 [US1] Add an "About" section with one flat button to `qml/pages/SettingsPage.qml` after the
  Account section, pushing `AboutPage.qml`; register `pages/AboutPage.qml` in `qml/qml.qrc`.
- [X] T022 [US1] Run `ctest --test-dir build-host --output-on-failure`; `app_self_test` must load the
  new page without QML errors; all earlier tests green.
- [ ] T023 [US1] [MANUAL] On the reader (quickstart §3, first two rows): About shows the version equal
  to the zip's version, author, copyright identical to `LICENSE`, license name; Back returns to
  Settings. Record the result in `specs/003-about-box/checklists/device-results.md`.

**Checkpoint**: US1 works on its own and is the MVP.

---

## Phase 4: User Story 2 - Read the license of the app (Priority: P2)

**Goal**: From About, open the full text of the app license, page through it, go back.

**Independent Test**: Open "License"; the MIT text and copyright line are shown; Page down reaches
the last line; Back returns to About; works offline.

### Tests first (observe failing)

- [X] T024 [P] [US2] Write `tests/unit/test_about_text.cpp` for `load_text` and `split_paragraphs`
  (`contracts/core-library.md`): `split_paragraphs("a\nb\n\n\nc")` → `["a\nb","c"]`; `""` and
  `"\n\n"` → `[]`; CRLF normalised; trailing spaces trimmed; more than 5,000 paragraphs → capped at
  5,000. `load_text`: all files readable → paragraphs in order, with a heading paragraph (file base
  name without extension) before each file only when there are several; some unreadable → still
  `available`; none readable → `available == false`; empty `text` → `available == false`; file over
  256 KiB → cut on a line boundary, `truncated`, last paragraph `Text shortened.`; path rules:
  absolute path, `..`, backslash, NUL → treated as unreadable; a symlink inside the root pointing
  outside it → unreadable.
- [X] T025 [P] [US2] Extend `tests/bridge/test_about_bridge.cpp` with `appLicense()`: with the fixture
  text → `available`, `title`, `header` lines, `paragraphs`; text file missing → `available == false`
  and a single paragraph `The license text could not be read.`; no manifest → `available == false`.

### Implementation

- [X] T026 [US2] Add `LicenseText`, `split_paragraphs` and `load_text` to `core/include/pbnotes/about.h`
  and implement them in `core/src/about.cpp` (real-path check with `std::filesystem::weakly_canonical`
  below the root; reads at most 256 KiB per file; logs `about.text_unavailable` as a count). Make T024
  pass.
- [X] T027 [US2] Add `appLicense()` to `AboutBridge` (`bridge/include/about_bridge.h`,
  `bridge/src/about_bridge.cpp`) returning `{title, header, paragraphs, available}`; header lines
  `Holder: …` and `License: …`. Make T025 pass.
- [X] T028 [P] [US2] Create `qml/pages/LicenseTextPage.qml`: input `{title, header, paragraphs,
  available}`; the header lines first (bold labels), then a `ListView` of paragraphs (26 px,
  `Text.PlainText`, wrap, black on white) with `PageButtons`, and "Back". Remember the index of the
  top visible paragraph and call `positionViewAtIndex` after a width/height change so rotation keeps
  the place (FR-015). No animation.
- [X] T029 [US2] Add a "License" button to `qml/pages/AboutPage.qml` pushing `LicenseTextPage.qml`
  with `about.appLicense()`; register `pages/LicenseTextPage.qml` in `qml/qml.qrc`.
- [X] T030 [US2] Run `ctest --test-dir build-host --output-on-failure`; `app_self_test` green.
- [ ] T031 [US2] [MANUAL] On the reader, Wi-Fi off (quickstart §3 "License" and rotation rows): full
  text, paging to the end, Back to About, rotation keeps the place. Record in
  `specs/003-about-box/checklists/device-results.md`.

**Checkpoint**: US1 and US2 both work independently.

---

## Phase 5: User Story 3 - See the licenses of the libraries and resources (Priority: P2)

**Goal**: A list of every third-party component with its license, a text page per entry, and a
build-time check that the list cannot drift from `NOTICE`, `third_party/` and the CMake
dependencies.

**Independent Test**: Open "Third-party licenses"; every component of `NOTICE` is listed with a
license name; each entry opens its holder, license, address and text (or the address fallback);
`ctest -R test_license_consistency` fails when a dependency is added without an entry.

### Tests first (observe failing)

- [X] T032 [P] [US3] Write `tests/support/license_check.h` (function
  `std::vector<std::string> check_consistency(const Manifest&, std::string_view notice,
  const std::vector<std::string>& third_party_dirs, const std::vector<std::string>& cmake_names,
  <file-exists / size callback>, std::string_view license_file)` returning findings, each starting with
  `rule N:` and naming the entry) and `tests/unit/test_license_consistency.cpp`. Rules, verbatim from
  `contracts/manifest-format.md`: (1) every `component` block has a `NOTICE` section whose heading line
  starts with the block's `name`, and every `NOTICE` section (text after a line of 80 dashes) has a
  block; (2) every directory directly under `third_party/` appears in some block's `source`; (3)
  every `find_package(<Name>` and linked library in the non-test CMake files (other than the project's
  own targets, `pthread`, `m`) appears in some block's `cmake`; (4) every `text` and `source` path
  exists, every `text` file is non-empty and ≤ 256 KiB; (5) the app block's `copyright` equals the
  first `Copyright` line of `LICENSE`; (6) the manifest has no bad blocks. Negative cases on
  in-memory data prove each rule fires (one test per rule, message names rule and entry). One
  positive test runs the check on the REAL repository: manifest, `NOTICE`, directory listing of
  `third_party/` (`std::filesystem`), names scraped from `CMakeLists.txt`, `core/`, `bridge/`,
  `app/`, `cmake/` (not `tests/`). It fails until T035.
- [X] T033 [P] [US3] Extend `tests/bridge/test_about_bridge.cpp`: `components` has the valid
  components only, `bundled` entries first then `system`, manifest order inside a group, each item
  `{id, name, purpose, license, origin}`; `unreadableEntries` equals `bad_blocks`;
  `componentDetail(id)` for a component with text → `available`, header lines (`Holder`, `License`,
  `Origin`, `Address`, `Purpose`) and paragraphs; with empty `text` → `available == true` and the
  single paragraph `The full license text is published at <url>.`; with text files that are all
  missing → `available == false` and `The license text could not be read.` while the header lines are
  still returned; unknown id → empty map; no manifest → empty list and `unreadableEntries == 0`;
  data directory unchanged after calling every member.

### Implementation

- [X] T034 [US3] Update `NOTICE`: add one section each, separated by the 80-dash line used there, for
  `Qt 6.8 (Core, Gui, Qml, Quick) (LGPL-3.0)`, `libcurl (curl license)`, `OpenSSL (Apache-2.0)`,
  `PocketBook InkView (terms set by PocketBook)`, `GNU C Library (LGPL-2.1-or-later)`, `GCC runtime
  libraries (GPL-3.0-or-later with the GCC Runtime Library Exception 3.1)`. Each states that the
  component is provided by the reader's system software and not redistributed by this project,
  names the holder and address from the manifest, and for Qt adds that it is dynamically linked and
  that Qt includes further components under their own licenses (see Qt's documentation). The
  InkFeed heading stays `InkFeed (MIT License)`. No invented license name for InkView.
- [X] T035 [US3] Make T032 pass: if findings come from naming (rule 1) fix `licenses/manifest.txt`
  names or `NOTICE` headings, not the check. Register the test through the existing glob (rerun
  CMake).
- [X] T036 [US3] Add `components`, `unreadableEntries` and `componentDetail(id)` to `AboutBridge`
  (`bridge/include/about_bridge.h`, `bridge/src/about_bridge.cpp`) per `contracts/qml-bridge.md`;
  detail text uses `load_text`; a component with several text files shows them in order. Make T033
  pass.
- [X] T037 [P] [US3] Create `qml/pages/ThirdPartyPage.qml`: title "Third-party licenses"; a `ListView` of
  rows with group labels `qsTr("In the app package")` and `qsTr("Provided by the reader")`; each row a
  flat button with **name** (bold), purpose and `License: <license>`; tapping pushes
  `LicenseTextPage.qml` with `about.componentDetail(id)`; footer `N entries could not be read.` when
  `about.unreadableEntries > 0`; `PageButtons` with `snap` (whole rows) and "Back".
- [X] T038 [US3] Add a "Third-party licenses" button to `qml/pages/AboutPage.qml` pushing
  `ThirdPartyPage.qml`; register `pages/ThirdPartyPage.qml` in `qml/qml.qrc`.
- [X] T039 [US3] Run `ctest --test-dir build-host --output-on-failure`; `app_self_test` green.
- [ ] T040 [US3] [MANUAL] On the reader, Wi-Fi off (quickstart §3 rows "Third-party licenses", "Open
  each entry", "Hold a page button"; §3a degraded installs): list, every entry, SQLite/InkView
  address fallback, GPL-3.0 to its last line, rotation, missing file and missing manifest cases.
  Record in `specs/003-about-box/checklists/device-results.md`.

**Checkpoint**: all three stories work independently and together.

---

## Phase 6: Polish & cross-cutting concerns

- [X] T041 [P] Update `README.md` (status/feature list: the About screen, where the license texts live,
  how to add a dependency: add a manifest block and a `NOTICE` section) and `INSTALL.md` (the package
  now contains `pbnotes-licenses/`; it must be copied with `applications/`).
- [X] T042 Run `scripts/pre-merge.sh`: all tests, constitution checks (R5 no absolute paths in the new
  files, R6 no forbidden log keys), fixture check and secret scan green.
- [X] T043 Walk through quickstart §1a (the six drift edits) and confirm each makes
  `test_license_consistency` fail with the rule and entry named; revert each edit. Record the result
  in `specs/003-about-box/checklists/device-results.md`.
- [ ] T044 [MANUAL] Release check (quickstart §4): `NOTICE` reviewed against the SDK's current library
  versions, About version equals the release identifier, the three [VERIFY] facts of `research.md`
  R-2 settled. Record in `specs/003-about-box/checklists/device-results.md`.
- [X] T046 Release 1.0.0: `VERSION` file as the single version source for CMake and
  `scripts/package-device.sh`, `CHANGELOG.md`, `scripts/check-version-bump.sh` (merge gate, self-test in
  CTest), constitution 1.3.0 and `tasks-template.md` carry the release task for later features.
- [X] T045 Update `specs/003-about-box/checklists/requirements.md` notes: all FRs covered by tasks; list
  the open [MANUAL] tasks.

---

## Dependencies & Execution Order

- Phase 1 (T001–T005) has no dependencies; T001–T003 are parallel, T004 can run in parallel with them,
  T005 needs all four.
- Phase 2 needs Phase 1 only for T012 (texts exist) and T013 (packaging). T006/T007 first, then
  T008 → T009 → T010; T011 is independent; T012 needs T005, T004; T013 needs T012; T014 needs T013.
- US1 (Phase 3) needs Phase 2. US2 (Phase 4) needs US1's `AboutBridge` and `AboutPage` (T018, T020).
  US3 (Phase 5) needs US1's bridge and page and US2's `LicenseTextPage` (T028) for the detail view;
  its core/NOTICE/consistency work (T032, T034, T035) needs only Phase 2 and can start earlier.
- Within a story: tests, observed failing → core → bridge → QML → run tests → manual check.
- Polish needs the stories it documents or runs.

### Parallel opportunities

- Phase 1: T001, T002, T003, T004 together.
- Phase 2: T006, T007 (tests) together; T011 alongside T008–T010.
- US1: T015 and T016 together; T020 alongside T017–T019.
- US2: T024 and T025 together; T028 alongside T026–T027.
- US3: T032 and T033 together; T034 alongside T036; T037 alongside T036.
- US3's T032/T034/T035 can run in parallel with US2.

## Implementation Strategy

- **MVP**: Phase 1 + Phase 2 + User Story 1 (T001–T023): the version, author, copyright and license name
  are visible in the app and the package carries the manifest and texts. Stop and validate on the reader.
- **Increment 2**: User Story 2 (full app license text and the paged text page).
- **Increment 3**: User Story 3 (third-party list, the `NOTICE` update, the drift check). Until it lands
  the `NOTICE` file is incomplete for the system libraries, so do not tag a release before T034/T035.
- Every task that changes behaviour follows red → green; a task that adds a test also states that the
  test must be seen failing before the matching implementation task starts.
