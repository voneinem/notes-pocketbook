# Specification Quality Checklist: Autosync on Save

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-10-12
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- Validated in one pass; no clarification markers were needed. Defaults chosen (on by default,
  all saved changes count as "save", no retry timer, close saves combined) are recorded under
  Assumptions.
- FR-020 deliberately refines FR-031 of feature 001 (sync only on request or at app start) by
  adding an opt-in third trigger.

## Coverage by task (implementation, 2026-10-12)

Every FR-001..FR-020 is covered by tasks in `tasks.md`:

| Requirement | Tasks | Evidence |
|-------------|-------|----------|
| FR-001, FR-002, FR-004 (control, default on, persisted) | T002, T003, T006–T009 | `test_store_open`, `test_account_store`, `test_store_migration_v3`, `test_notes_bridge` (setting), `settings_sync.qml` |
| FR-003 (effective at once) | T006, T016 | `test_notes_bridge` (setting persists/announces; switching off mid-sync) |
| FR-005 (no account) | T010, T015 | `without_an_account_a_save_starts_no_sync_and_reports_no_error`, `settings_sync.qml` |
| FR-006..FR-008 (what triggers) | T010–T012 | `test_notes_bridge`: edit, new note, delete, favorite, category, task box |
| FR-009 (same sync) | T014 | conflict case and Markdown byte-for-byte case in `test_notes_bridge` |
| FR-010 (off = no sync) | T010 | `with_the_switch_off_a_save_sends_nothing` |
| FR-011 (only a completed save), editor deferral | T004, T015, T017 | `test_sync_scheduler`, `no_automatic_sync_starts_while_an_editor_is_open` |
| FR-012 (never waits for the network) | T010 | `a_saved_edit_is_sent_by_an_automatic_sync` (no request at return) |
| FR-013 (offline is quiet) | T004, T015 | `without_wifi_a_save_is_kept_quietly_and_goes_out_with_the_next_sync` |
| FR-014 (one at a time, one follow-up) | T004, T015 | `a_save_while_a_sync_runs_is_sent_by_exactly_one_more_sync` |
| FR-015 (saves combined) | T004, T015 | `saves_close_together_share_one_sync`, scheduler ten-saves case |
| FR-016 (failure reported, no retry loop) | T015 | `a_failed_automatic_sync_is_reported_in_the_status_and_not_retried` |
| FR-017 (conflicts keep both versions) | T014, T018–T021 | conflict case; `saving_stale_editor_text_after_a_pull_keeps_both_versions`; `test_store_edit` |
| FR-018 (status shown) | T010, T029 | status text asserted; device walkthrough |
| FR-019 (verified connection, no text in logs) | T011, T025 | reviewed in T025: log lines carry codes and counts only; the transport is untouched |
| FR-020 (no timer/background sync) | T011, T024, T025 | the only timer serves a wait the scheduler asked for after a save; feature 001 FR-031 carries a pointer |

Added beyond the spec text, because the feature makes it necessary (research R-6): a save from an editor whose note a
sync replaced meanwhile is kept as a conflict copy (T018–T021). The guard compares text, category and favorite, not the
ETag, so the reader's own confirmed push is no conflict.

Open `[MANUAL]` task: **T029**, the device walkthrough of `quickstart.md` §3 (13 rows) on the reader with the release
package, including the timings of SC-001 and SC-006.
