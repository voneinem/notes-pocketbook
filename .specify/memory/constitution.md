# Notes Pocketbook Constitution

## Core Principles

### I. Test-First (NON-NEGOTIABLE)
Tests MUST be written before the implementation they cover, and MUST be observed failing before
the implementation is written. The Red-Green-Refactor cycle is followed for every feature and bug
fix. No change is merged while any test fails.

Rationale: notes are user data; regressions silently lose or corrupt it. Failing-first tests prove
the tests can actually detect the defect.

### II. Simplicity (YAGNI)
Implement only what the current specification requires. Speculative abstractions, configuration
options, and dependencies MUST NOT be added. Any added complexity (new dependency, new layer,
new component) MUST be justified in the plan's complexity-tracking section, naming the simpler
alternative that was rejected and why.

Rationale: a small, readable codebase is easier to secure, test, and maintain.

### III. Security & Privacy
User notes and highlights are private by default. The project MUST collect only the data needed
for its stated function, MUST NOT transmit user data to third parties without explicit user
consent, and MUST NOT commit secrets, credentials, or real user data to the repository. All
external input (device files, imports, network requests) MUST be validated before use.

TLS certificate verification MUST be enabled for every connection that carries credentials.
Disabling verification, including as a development or device workaround, MUST NOT be implemented;
InkFeed's disabled-verification workaround MUST NOT be copied. Trust anchors MUST come from the
device CA store when it is usable, otherwise from a CA file bundled with the application. If a
file `custom-ca.pem` exists in the application data directory, its certificates are added to
those trust anchors. Certificate verification MUST remain enabled in all cases, no option may
trust unverifiable certificates, and the Settings screen MUST show whether a custom CA is loaded.
An explicit "trust self-signed certificate" option MAY exist only if it is off by default, applies
to a single user-chosen server, and shows a clear warning when enabled.

Rationale: notes and reading annotations reveal personal information, and credentials sent over an
unverified connection can be intercepted; trust depends on conservative handling.

### IV. Integration Testing at Device Boundaries
Behavior at system boundaries MUST be covered by integration tests: reading and writing
PocketBook note/highlight data, import/export formats, and any storage or sync layer. Tests MUST
use anonymized fixtures, never real user data. Changes to a boundary format or contract MUST
include a contract test.

Rationale: the device's data formats are the main external dependency and the most likely source
of breakage.

### V. Host-Testable Core
Sync, API, and storage logic MUST be buildable and unit-testable on a desktop host without the Qt
UI and without the device or its SDK. This logic MUST NOT depend on Qt Quick, InkView, or other
device-only libraries. The build MUST provide a host (desktop) target that compiles and runs this
logic and its tests.

Rationale: fast, device-free tests make Test-First practical and keep the core portable.

### VI. No Silent Data Loss
User data MUST NOT be lost or silently overwritten. Writes MUST send If-Match where the API
supports conditional writes. For DELETE, first perform a conditional GET with the stored ETag and
delete only if the note is unchanged on the server; a changed note is a conflict and is not
deleted. An HTTP 412 response (or any other detected conflict) MUST be treated as a conflict. On
conflict both versions MUST be kept and the user MUST be able to resolve it; neither version may be
discarded automatically. Local changes MUST NOT be discarded until the server has confirmed them.

Rationale: a reference implementation was observed to push without If-Match, which lets the last
writer win and discard the other edit; for a notes app that is unacceptable. DELETE is guarded by a
conditional GET because the server may not honor If-Match on DELETE.

## Security & Data Handling Constraints

- Data MUST NOT be deleted or overwritten on a source device or file without explicit user
  confirmation; operations on source data are read-only by default.
- Dependencies MUST be pinned and reviewed before being added (see Principle II).
- Logs MUST NOT contain note contents or other personal data.

## Technology Stack & Build Constraints

- The stack follows `docs/inkfeed-analysis.md`: C++17, Qt 6.8 Quick/QML for the UI, CMake as the
  build system, libcurl for networking, and SQLite for storage. Changing any of these requires a
  constitution amendment.
- The location of the PocketBook SDK MUST come only from the `POCKETBOOK_SDK` environment
  variable or the `-DSDK_PATH` CMake option. SDK or toolchain paths MUST NOT be hard-coded in
  build files, scripts, or documentation examples.
- The build MUST provide both a device target (ARM, via the SDK) and a host target (Principle V).

## Licensing & Attribution

- Build, launcher, and packaging patterns MAY be adapted from InkFeed (MIT license). Each adapted
  pattern MUST be attributed in a `NOTICE` file in the repository.
- notes-android (GPL-3.0) is reference-only: no code, strings, comments, or documentation text
  from it MAY be copied. Behavior MAY be reimplemented from the public Nextcloud Notes API
  specification and from behavior-only descriptions such as `docs/notes-android-features.md`.
- The project license is the MIT License. The `LICENSE` file at the repository root states
  "Copyright (c) 2026 Volker von Einem". The README MUST state the license before the first
  public push.

## Development Workflow

- Every change goes through review; the reviewer MUST verify compliance with this constitution.
- A feature follows the Spec Kit flow: specification → plan → tasks → implementation, with the
  plan's Constitution Check passing (or deviations justified) before design proceeds.
- Test gates: all tests pass and no new secrets are detected before merge.
- Release version: every merged change to the product raises `VERSION` (MINOR for a feature, PATCH for
  a fix, MAJOR for a breaking change such as an irreversible data upgrade) and adds an entry to
  `CHANGELOG.md`. The bump is a task of the feature (last phase of `tasks.md`) and is enforced by
  `scripts/check-version-bump.sh` in `scripts/pre-merge.sh`. After the merge the release is tagged
  `v<version>`. Changes nobody can see (refactoring) may opt out with `PBNOTES_NO_BUMP=1`, stated in
  the pull request.
- Reviewers MUST reject changes that add TLS-verification bypasses, hard-coded SDK paths, or code
  copied from notes-android.

## Governance

This constitution supersedes other project practices. Amendments MUST be made by editing this
file with a documented rationale and a version bump, and MUST include any migration steps needed
for existing specs, plans, or code.

Versioning follows semantic versioning: MAJOR for backward-incompatible removal or redefinition
of principles or governance; MINOR for a new principle or section or materially expanded
guidance; PATCH for clarifications and wording fixes.

Compliance is reviewed in every pull request and in each plan's Constitution Check. Unjustified
violations block the merge.

**Version**: 1.3.0 | **Ratified**: 2026-10-10 | **Last Amended**: 2026-10-11
