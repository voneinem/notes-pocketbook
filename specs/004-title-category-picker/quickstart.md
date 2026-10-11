# Quickstart: validating the explicit title and the category picker

Validation guide only; implementation details live in `tasks.md`. Contracts:
[core-library](contracts/core-library.md), [notes-api-title](contracts/notes-api-title.md),
[qml-bridge](contracts/qml-bridge.md). Data: [data-model](data-model.md). Decisions:
[research](research.md). Build, packaging and device prerequisites are those of
`specs/001-pocketbook-notes-mvp/quickstart.md` §1–§3.

## 1. Host build and tests (no device)

```bash
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host
ctest --test-dir build-host --output-on-failure
scripts/pre-merge.sh
```

Expected: all suites pass offline. New or changed tests are those of research R-9; each is written
first and observed failing before its implementation (constitution I). The app `--self-test`
(`app_self_test` in CTest) must still pass with the changed editor page.

## 2. Migration check (no device)

1. Take the anonymized version-2 database fixture used by `test_store_migration_v3` (an unsynced
   note titled `## Plan`, a synced note whose server title differs from its first line, a note
   whose first line is not its title).
2. Open it with the new build (`ctest -R migration_v3`). Expect: version 3; the unsynced note's
   title is `Plan`; the synced note's title is untouched; previews recomputed; no `.bak` left; no
   row's status, modified, etag, remote id, category or content changed.
3. Re-run with the injected failure hook: the database is left at version 2 and the backup is kept.

## 3. Device walkthrough (InkPad One, real server)

Prerequisite: a disposable Nextcloud account/notes with a few notes in categories "Home" and "Work".
Install per the standing instructions for the mounted reader, then:

| # | Step | Expected |
|---|------|----------|
| 1 | Open a note, edit its first text line, Save | list and reader still show the old title (US1-2) |
| 2 | Open it, change only the title, Save | new title in list/reader, text unchanged (US1-4) |
| 3 | New note: type text, leave the title empty, Save | not saved; message "The note needs a title."; text still there (US1-5) |
| 4 | Edit the title, press Cancel | "Discard changes?" appears (US1-6) |
| 5 | Open Category on a note | list: No category, New category…, Home, Work; current marked ✓ (US2-1) |
| 6 | Choose Work, Save | note is in the Work view; no keyboard was needed (US2-2) |
| 7 | Choose No category, Save | note appears under Uncategorized (US2-3) |
| 8 | Open Category, press Cancel | category unchanged (US2-4) |
| 9 | New category…, type `Travel`, Confirm, Save; open another note's Category | `Travel` is listed (US3-2/3) |
| 10 | New category…, type `Work` | selects the existing Work; no duplicate (US3-4) |
| 11 | New category…, leave empty / 256 characters | message, nothing assigned (US3-5) |
| 12 | New category `Draft`, leave the editor with Discard; open any Category list | `Draft` is not listed (US3-7) |
| 13 | Create 30+ categories (scripted or via several notes), open Category | page buttons reach every entry; No category and New category… are on page 1 (US2-5, SC-006) |
| 14 | Airplane mode: change a title and a category, Save; reconnect, sync | both reach the server; web app shows them (US1-7, SC-005) |
| 15 | Change a note's title in the web app, sync | reader shows it (US1-8) |
| 16 | **[VERIFY]** Give two notes of one category the same title; add a title with `/` | server's reported titles are shown after sync; no note lost (R-3) |
| 17 | Edit the same note's title on the web app and on the reader before syncing | two notes after sync: server version and ` (conflict …)` copy with the reader's title; nothing lost |

Record any deviation in the feature's walkthrough note (as `docs: record the device walkthrough`
commits do for earlier features).
