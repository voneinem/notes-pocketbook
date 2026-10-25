# Feature Specification: PB Store Release Package

**Feature Branch**: `feature/011-pb-store-package`

**Created**: 2026-10-24

**Status**: Draft

**Input**: User description: "Build the PB Notes release package so that it follows the PB Store package contract (appstore-pocketbook, tag v0.1.1). The PB Store lists apps only if their release ZIP follows the contract. Checked with `pbstore-catalog check-package`, the 1.10.0 release trips exactly two rules: E-PKG-04 (INSTALL.md at the ZIP root; only manifest.json and the applications/ tree are allowed there) and E-PKG-05 (no manifest.json). Everything else already conforms. Needed: a manifest.json at the ZIP root created at build time from a template in the repository; INSTALL.md no longer at the ZIP root; the package built with `pbstore-catalog pack` (reproducible, checked while it is built), the tool found through PATH or an environment variable, a clear failure when it is missing; the built package passes `pbstore-catalog check-package`; the file name stays pbnotes-inkpad-one-<version>.zip and the files under applications/ stay the same so a manual install keeps working; the release is tagged v<version> and the ZIP attached to the GitHub release, the tag equals the manifest version. Out of scope: the store entry in appstore-pocketbook, PB Cloud and PB Office, any change of the app itself."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - The release package is accepted by the PB Store contract check (Priority: P1)

The maintainer builds the release package for the current version. The package carries a
manifest at its top level that describes PB Notes for the store (identity, name, version,
minimum firmware, target device, menu entry, data folder), and nothing else sits at the top
level except the `applications/` tree. The store's own package checker finds no violation.

**Why this priority**: The store lists an app only if its release package follows the contract.
Today two rules are broken (E-PKG-04: a stray INSTALL.md at the top level; E-PKG-05: no
manifest), so PB Notes cannot be listed at all. Fixing exactly these two is the whole point of
the feature and is useful on its own.

**Independent Test**: Build the package for the current version, run the store's package
checker on the resulting ZIP, and verify it reports no violation. Open the ZIP and verify that
its top level holds only the manifest and the `applications/` folder, and that the manifest
shows the agreed values and the version from the `VERSION` file.

**Acceptance Scenarios**:

1. **Given** a built package, **When** the maintainer runs the store's package checker on it,
   **Then** the checker passes with no error, in particular neither E-PKG-04 nor E-PKG-05.
2. **Given** the `VERSION` file says `1.10.1`, **When** the package is built, **Then** the
   manifest inside the ZIP names version `1.10.1` and the ZIP is named
   `pbnotes-inkpad-one-1.10.1.zip`.
3. **Given** a built package, **When** its top level is listed, **Then** it contains only the
   manifest and the `applications/` folder.
4. **Given** the manifest values in the repository, **When** the package is built, **Then** the
   manifest states: schema version `1.0`, id `pbnotes`, name `PB Notes`, minimum firmware
   `6.11.1437`, target device `InkPad One`, menu entry id `U_PbNotes`, title `PB Notes`, launcher
   `applications/pbnotes.app`, icon `applications/icons/pbnotes.bmp`, focused icon
   `applications/icons/pbnotes_f.bmp`, group `@General`, data folder `system/pbnotes`.

---

### User Story 2 - The package is built reproducibly with the store's tool, or fails clearly (Priority: P2)

The maintainer builds the package with the store's own packing tool, so the ZIP is laid out the
way the store expects. Building the same files twice yields the identical ZIP, and the build
verifies this itself. The tool is located through the search path or an environment variable;
no machine-specific location is written into the repository. When the tool cannot be found, the
build stops with a message that says what is missing and how to point the build at it.

**Why this priority**: A package that is only correct by accident of how it was zipped can drift
away from the contract. Using the store's tool and proving reproducibility makes the result
trustworthy and the store entry's checksum stable. It builds on User Story 1 and is second.

**Independent Test**: Build the package twice from the same files and compare the two ZIPs byte
for byte. Then build once with the tool neither on the search path nor named by the environment
variable and verify the build stops with a clear message and leaves no half-written ZIP behind.

**Acceptance Scenarios**:

1. **Given** the packing tool is on the search path, **When** the package is built twice from
   unchanged files, **Then** both ZIPs are byte-identical and the build reports the
   reproducibility check as passed.
2. **Given** the packing tool is not on the search path but the environment variable names it,
   **When** the package is built, **Then** the build uses that tool and succeeds.
3. **Given** the tool is neither on the search path nor named by the environment variable,
   **When** the package is built, **Then** the build fails before producing a ZIP, and the message
   states that the tool is missing and how to make it available.
4. **Given** the two builds in the reproducibility check produce different ZIPs, **When** the
   check runs, **Then** the build fails and says so, and no ZIP is left in the output folder as a
   valid result.

---

### User Story 3 - Manual installation keeps working and its instructions stay reachable (Priority: P2)

A reader who installs by hand (unzip onto the reader's storage, register the app in the
Applications menu) still gets exactly the files under `applications/` they get today, under the
same name, so the manual steps do not change. The install instructions no longer sit at the top
level of the ZIP, but a reader can still find them.

**Why this priority**: Manual installation is how PB Notes is used today, and the store route is
new and unproven; it must not break the existing one. It has the same priority as the build
guarantees because removing INSTALL.md from the top level otherwise strands manual installers.

**Independent Test**: Compare the file list under `applications/` of the new ZIP with that of
the previous release (1.10.0): the same files at the same paths. Starting from the GitHub
release page and from the README, verify a reader can find and follow the install instructions
without any file in the ZIP.

**Acceptance Scenarios**:

1. **Given** the previous and the new package, **When** their `applications/` contents are
   compared, **Then** the same set of files exists at the same paths.
2. **Given** the new package, **When** a reader unzips it onto the reader's storage as before,
   **Then** the app starts after the usual menu registration, and the registration steps still
   match what is needed.
3. **Given** the new package and its GitHub release, **When** a reader looks for install
   instructions, **Then** they find them in the repository (`INSTALL.md`, linked from
   the README) and in the notes of the GitHub release; the ZIP itself carries no copy.

---

### User Story 4 - The release is tagged and published with the package attached (Priority: P3)

When a version is released, the repository is tagged with the version prefixed by `v`, and the
package is attached to the GitHub release of that tag. The tag, the version in the manifest and
the version in the ZIP name are the same number, so the store can find the right package for a
version without guessing.

**Why this priority**: The store resolves a version to a release by its tag, so a mismatch makes
the package unfindable or wrong. It comes last because it concerns the act of releasing rather
than the package itself; the first two stories deliver a verifiable package without it.

**Independent Test**: For a release, read the tag name, the manifest version inside the attached
ZIP and the version in the ZIP's file name and verify all three denote the same version. Download
the ZIP from the GitHub release and run the store's package checker on it.

**Acceptance Scenarios**:

1. **Given** version `1.10.1` is released, **When** the release is published, **Then** the tag is
   `v1.10.1` and the ZIP `pbnotes-inkpad-one-1.10.1.zip` is attached to the GitHub release of
   that tag.
2. **Given** a tag `vX.Y.Z` and a package whose manifest names a different version, **When** the
   release is prepared, **Then** the mismatch is detected and the release does not go ahead.
3. **Given** the attached ZIP downloaded from the release, **When** the store's package checker
   runs on it, **Then** it passes.

---

### Edge Cases

- The `VERSION` file is missing, empty or not a plain version number: the build stops with a
  message instead of writing a manifest with a bad version.
- The manifest template is missing, or a value in it is left unfilled after the build: the build
  stops instead of producing a package with an incomplete manifest.
- A file that the manual install needs (launcher, program, icons, bundled certificates, emoji
  font, license texts) is missing at build time: the build stops with the name of the missing
  input, as it does today.
- The binary does not contain the version from `VERSION`: the build stops, as it does today, so
  the ZIP name and the About screen cannot disagree.
- A ZIP of the same version already exists in the output folder: it is replaced by the new build,
  never merged with it.
- The store's checker is not installed: the build fails clearly (as for the packing tool); the
  checker's verdict is a required step of the build, not an optional extra.
- The packing tool changes its rules in a later version and the package no longer passes: the
  build fails rather than publishing a package the store will reject.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The release package MUST contain a manifest file at its top level, and the only
  entries at the top level MUST be that manifest and the `applications/` folder.
- **FR-002**: The manifest MUST be generated at build time from a template kept in the
  repository, not written by hand per release; the template holds every value that does not
  change between releases.
- **FR-003**: The manifest MUST state: schema version `1.0`, app id `pbnotes`, name `PB Notes`,
  the version read from the `VERSION` file, minimum firmware `6.11.1437`, target device
  `InkPad One`, and one menu entry with id `U_PbNotes`, title `PB Notes`, launcher path
  `applications/pbnotes.app`, icon path `applications/icons/pbnotes.bmp`, focused icon path
  `applications/icons/pbnotes_f.bmp`, group `@General`, and data folder `system/pbnotes`.
- **FR-004**: The version in the manifest MUST come from the same single source (the `VERSION`
  file) as the ZIP file name and the version compiled into the app; the build MUST NOT let them
  disagree.
- **FR-005**: The install instructions MUST NOT be part of the package. They MUST remain in the
  repository (`INSTALL.md`, linked from the README) and be included in the notes of each GitHub
  release, so a reader who installs manually can still find them.
- **FR-006**: The package MUST be assembled with the store's packing tool, and the build MUST
  verify reproducibility by producing the package from the same files twice and requiring
  identical results.
- **FR-007**: The build MUST find the packing tool (and the checker) through the search path or
  through an environment variable, and MUST NOT contain a machine-specific location of it.
- **FR-008**: When the packing tool or the checker cannot be found, the build MUST fail before
  producing a package, with a message that says which tool is missing and how to make it
  available.
- **FR-009**: The build MUST run the store's package checker on the finished ZIP and MUST fail,
  without leaving a ZIP that looks valid in the output folder, if the checker reports any error.
- **FR-010**: The package file name MUST remain `pbnotes-inkpad-one-<version>.zip`.
- **FR-011**: The set of files under `applications/` and their paths MUST stay the same as in the
  1.10.0 package, so that unzipping onto the reader's storage and registering the app by hand
  works exactly as before, with nothing added or removed.
- **FR-012**: A release MUST be tagged `v<version>`, with `<version>` equal to the version in the
  manifest, and the ZIP MUST be attached to the GitHub release of that tag.
- **FR-013**: The release procedure MUST detect and refuse a tag that differs from the manifest
  version.
- **FR-014**: The project documentation that describes building and releasing the package MUST be
  updated to describe the new steps, the tool lookup and the environment variable.
- **FR-015**: Nothing in the app itself, in the store entry in the `appstore-pocketbook`
  repository, or in PB Cloud and PB Office changes as part of this feature.

### Key Entities

- **Release package**: the ZIP the reader or the store downloads for one version; holds the
  manifest and the `applications/` tree.
- **Package manifest**: the description of the app for the store (identity, version, minimum
  firmware, target device, menu entry, data folder); generated per build from the template and
  the `VERSION` file.
- **Manifest template**: the file in the repository holding the manifest values that do not
  change between releases.
- **Release**: a version tag `v<version>` on the repository together with its GitHub release and
  the attached package.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The store's package checker reports zero errors for the package of the next
  release (today: two, E-PKG-04 and E-PKG-05).
- **SC-002**: Building the package twice from the same files yields byte-identical ZIPs in 100%
  of builds, and the build proves this itself every time.
- **SC-003**: The `applications/` contents of the new package are identical in file list and
  paths to those of the 1.10.0 package; a reader following the manual steps needs no step that
  differs from today's.
- **SC-004**: For every release, tag, manifest version and ZIP name show the same version number
  (0 mismatches), and the package can be downloaded from the GitHub release of that tag.
- **SC-005**: With the packing tool unavailable, the build stops with a message naming the
  missing tool in under 5 seconds and produces no ZIP.
- **SC-006**: The maintainer can go from a finished build of the app to a store-valid package
  with a single command and no hand-edited files.

## Assumptions

- Decided in clarification: the install instructions are not shipped in the ZIP; the repository
  keeps `INSTALL.md` and each GitHub release's notes include the instructions (or a link to them).
- The author's InkPad One on firmware 6.11.1437 is the only tested device, so that firmware is
  the minimum and that model the only target; other models are untested and not claimed.
- The store's package contract is the one of `appstore-pocketbook` tag v0.1.1; a newer contract
  version is out of scope until adopted deliberately.
- The store's packing tool and checker are the same command-line program (`pbstore-catalog`),
  installed by the maintainer on the build machine; installing it is not part of this feature.
- The menu entry in the manifest describes the same entry that the manual install registers in
  the reader's desktop configuration (`U_PbNotes`, group `@General`), so the two routes agree.
- The existing version-bump rule applies: this feature changes the released artifact, so it raises
  `VERSION` and adds a `CHANGELOG.md` entry as the last phase of its tasks.
- Publishing the GitHub release remains a deliberate step done by the maintainer after the
  merge, as for earlier releases; this feature provides the correct package and the tag/version
  check, not an unattended publish.
- The store entry in `appstore-pocketbook`, PB Cloud and PB Office, and any change of the app
  itself are out of scope.
