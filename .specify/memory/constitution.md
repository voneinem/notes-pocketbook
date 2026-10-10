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

Rationale: notes and reading annotations reveal personal information; trust depends on
conservative handling.

### IV. Integration Testing at Device Boundaries
Behavior at system boundaries MUST be covered by integration tests: reading and writing
PocketBook note/highlight data, import/export formats, and any storage or sync layer. Tests MUST
use anonymized fixtures, never real user data. Changes to a boundary format or contract MUST
include a contract test.

Rationale: the device's data formats are the main external dependency and the most likely source
of breakage.

## Security & Data Handling Constraints

- Data MUST NOT be deleted or overwritten on a source device or file without explicit user
  confirmation; operations on source data are read-only by default.
- Dependencies MUST be pinned and reviewed before being added (see Principle II).
- Logs MUST NOT contain note contents or other personal data.
- The technology stack has not been chosen yet. When it is, this section MUST be amended to
  record it.

## Development Workflow

- Every change goes through review; the reviewer MUST verify compliance with this constitution.
- A feature follows the Spec Kit flow: specification → plan → tasks → implementation, with the
  plan's Constitution Check passing (or deviations justified) before design proceeds.
- Test gates: all tests pass and no new secrets are detected before merge.

## Governance

This constitution supersedes other project practices. Amendments MUST be made by editing this
file with a documented rationale and a version bump, and MUST include any migration steps needed
for existing specs, plans, or code.

Versioning follows semantic versioning: MAJOR for backward-incompatible removal or redefinition
of principles or governance; MINOR for a new principle or section or materially expanded
guidance; PATCH for clarifications and wording fixes.

Compliance is reviewed in every pull request and in each plan's Constitution Check. Unjustified
violations block the merge.

**Version**: 1.0.0 | **Ratified**: 2026-10-10 | **Last Amended**: 2026-10-10
