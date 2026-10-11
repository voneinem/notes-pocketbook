---

description: "Task list for Rich Note Content (links, tables, emojis, attachments)"
---

# Tasks: Rich Note Content (Links, Tables, Attachments, Emojis)

**Input**: Design documents from `/specs/002-rich-note-content/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: REQUIRED. The constitution makes Test-First non-negotiable (Principle I): every test task
below MUST be written and observed FAILING before the implementation task(s) it names. Tests for
`notes_core` run on the host without Qt, the SDK or a device (Principle V). Register each new test
executable in `tests/CMakeLists.txt` the way the existing ones are.

**Organization**: Tasks are grouped by user story. Paths are relative to the repository root. This
feature builds on the finished MVP (`specs/001-pocketbook-notes-mvp/`); all MVP tests must stay green.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US5 as in spec.md
- **[MANUAL]**: needs a person, the reader, or a live Nextcloud server; record the result in the named
  file. An implementing agent skips these and reports them as open

## Conventions that apply to every task

- C++17; namespace `pbnotes`; `notes_core` MUST NOT include Qt or `inkview.h` headers.
- TLS verification MUST never be disabled, SDK paths MUST NOT be hard-coded (see the MVP task list for
  the full forbidden-forms list, enforced by `scripts/check-constitution.sh`).
- Logs MUST NOT contain note text, link addresses, attachment names or file paths (spec FR-033); log
  counts, error classes and result words only.
- A path or file name that comes from a note or from the server MUST NEVER be used to build a local path;
  local attachment files are named `<id>.<ext>` only (data-model.md).
- Every file write for attachments goes to `<name>.part` first and is renamed when complete.
- The user's original files (add-attachment source) are only ever read.
- Do not copy code or text from notes-android; server behavior comes from the public Notes API
  description only.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Third-party asset, packaging, guards, a green baseline, and the server facts the design
still needs

- [ ] T001 [P] Add the monochrome emoji font as a pinned third-party asset: download `ofl/notoemoji/NotoEmoji[wght].ttf` and `OFL.txt` from the Google Fonts repository at a specific commit, store them as `third_party/noto-emoji/NotoEmoji.ttf` and `third_party/noto-emoji/OFL.txt`, and write `third_party/noto-emoji/VERSION` (source URL, commit hash, download date, SHA-256 of the font file, license SIL OFL 1.1)
- [ ] T002 [P] Update `NOTICE`: add an entry for Noto Emoji (SIL Open Font License 1.1, source URL and `third_party/noto-emoji/VERSION`, copyright line taken from the downloaded `OFL.txt`) and extend the InkFeed entry with the browser-launch pattern adapted in `app/device_bridge.cpp`
- [ ] T003 [P] Update `scripts/package-device.sh`: add `third_party/noto-emoji/NotoEmoji.ttf` and `third_party/noto-emoji/OFL.txt` to the input check and install them as `applications/pbnotes-emoji.ttf` and `applications/pbnotes-emoji-OFL.txt` (mode 644); update the header comment listing the zip layout (depends on T001)
- [ ] T004 [P] Update `scripts/check-constitution.sh` and its self-test samples: reject `log::text(`/`log::count(` keys named like `url`, `path`, `file`, `name` or `address` in `core/`, `bridge/` and `app/` (spec FR-033: no link addresses, attachment names or paths in logs); the self-test MUST show the rule fires on a bad sample and passes a good one
- [ ] T005 [MANUAL] Server probes (research R-15) against a real Nextcloud Notes server, using `curl` with an app password: (1) does `HEAD …/api/v1.4/attachment/<id>?path=.attachments.<id>/<name>` return `Content-Length`; (2) does `POST …/api/v1/notes` accept `{"content":""}`; (3) upload a file with `POST …/api/v1.4/attachment/<id>` (multipart field `file`) and record the exact JSON answer, including the name chosen when the same file is uploaded twice; (4) request an attachment that does not exist and one with a wrong note id; (5) note which HTTP status the server returns for a too-large upload if one can be provoked. Record answers in `research.md` R-15 and adjust the fixtures planned in T041 and T064 to the real shapes (anonymized)
- [ ] T006 Run the existing host suite as the baseline before any change: `cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-host && ctest --test-dir build-host --output-on-failure`; every test MUST pass; note the test count in the commit message of the first change so later regressions are visible

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Behavior-preserving refactors so the new features land in small files, and the one device
glue function both link and file opening need. No user-visible change in this phase.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete (`ctest` green, host app
still shows notes as before)

- [ ] T007 Split the inline parser out of `core/src/markdown.cpp` with no behavior change: move `add_span`, `plain_of`, `run_length`, `find_closer`, `read_link`, `parse_inline`, `inline_spans`, `literal_spans` into new `core/src/markdown_inline.cpp`, declare them in a private header `core/src/markdown_internal.h`, keep block parsing and `toggle_task_item` in `core/src/markdown.cpp`, add the new file to `core/CMakeLists.txt`; `tests/unit/test_markdown.cpp` and `tests/unit/test_markdown_toggle.cpp` MUST stay green unchanged
- [ ] T008 Move the block-to-map conversion out of `bridge/src/notes_bridge.cpp` (the loop in `NotesBridge::openNote` plus `spans_to_html` and `block_kind`) into new `bridge/include/reader_blocks.h` and `bridge/src/reader_blocks.cpp` exposing `QVariantList reader_blocks(const pbnotes::Note& note)`; `openNote` calls it; output MUST be byte-identical for existing blocks; add the file to `bridge/CMakeLists.txt`; `tests/bridge/test_notes_bridge.cpp` MUST stay green unchanged
- [ ] T009 Extract the text-block delegate (the `Row` with list marker/checkbox and the `Text`) from `qml/pages/ReaderPage.qml` into new `qml/BlockText.qml` (properties `block`, `fontPx`, signals `taskToggled(int line)` and `linkActivated(string link)`), add it to `qml/qml.qrc`, and use it from the reader's `Repeater`; behavior MUST be unchanged; verify by running the host app (`bridge` offscreen or desktop) and opening a note with headings, lists and a task item, toggling the task

**Checkpoint**: foundation ready, user stories can start. US1, US2 and US3 are independent of each
other (US1 and US2 both edit `ReaderPage.qml`, so do not run them in parallel on one working tree);
US4 follows US2 for the table-cell detail, US5 follows US4.

---

## Phase 3: User Story 1 - Follow a link from a note (Priority: P1) 🎯 MVP

**Goal**: Links in notes (titled, bare, autolinked) are visible as links; a tap opens the default
browser at once when the text is the address itself, otherwise after a confirmation that shows the real
address (spec FR-001 to FR-006).

**Independent Test**: quickstart §3 step 1 — open the test note, tap a bare address (opens at once),
a titled link (prompt shows the real address, Open launches the browser), a self-addressed link (opens
at once), and the `mailto:` link (plain text); returning shows the same note and position.

### Tests for User Story 1 ⚠️ write first, see them fail

- [ ] T010 [P] [US1] Write `tests/unit/test_links.cpp` for `is_followable_url` and `link_needs_confirmation` (contracts/core-library.md): followable = `http`/`https` (scheme in any letter case), non-empty host, no whitespace or control characters, at most 2048 bytes; rejects `ftp:`, `mailto:`, `javascript:`, `file:`, `data:`, relative paths, empty host, an address with a space, 2049 bytes. Confirmation rule (spec FR-002): returns false iff the link text is itself a web address naming the same destination after ignoring the scheme, a leading `www.`, the letter case of the host and one trailing `/`; true for `Read this`, for a different host, a different path, a different query, and for text that merely contains the address
- [ ] T011 [P] [US1] Write `tests/unit/test_markdown_links.cpp` for the inline parser: titled link keeps `Span.url` and `link_confirm`; emphasis inside the link text; bare `http(s)://` address becomes a link whose text is the address; trailing `. , ; : ! ? )` excluded from a bare address unless the parenthesis is balanced (`https://en.wikipedia.org/wiki/A_(b)` stays whole); `<https://x.example>` autolink; `[a](https://x.example "title")` ignores the title; targets that are not followable (`mailto:`, relative, `javascript:`) render as their label with no `url`; external image `![alt](https://x.example/p.png)` becomes a link with text `alt` (the address when `alt` is empty); an address inside an inline code span is not linked; two links in one line; links inside headings and list items; a 2049-byte address is not linked; adjacent spans with different `url` are not merged; `line_index` of all blocks is unchanged
- [ ] T012 [US1] Add link cases to a new `tests/bridge/test_reader_blocks.cpp` (register it): `reader_blocks()` html contains `<a href="l0"><u>…</u></a>` for a link and the block's `links` list holds `{url, confirm}` at that index; the raw address NEVER appears inside `html`; `<`, `>`, `&` and quotes in link text and addresses are escaped; a non-followable link shows only its label

### Implementation for User Story 1

- [ ] T013 [US1] Create `core/include/pbnotes/links.h` and `core/src/links.cpp` implementing `is_followable_url` and `link_needs_confirmation` exactly as specified in T010; add to `core/CMakeLists.txt`; T010 MUST pass
- [ ] T014 [US1] Extend `core/include/pbnotes/markdown.h` (`Span` gains `std::string url` and `bool link_confirm`) and `core/src/markdown_inline.cpp` (keep the link target in `read_link` including balanced parentheses and an optional trailing title, bare-address detection, `<…>` autolinks, external images as links, followable check via `links.h`, span merging only when kind and url match); T011 MUST pass and `tests/unit/test_markdown*.cpp` stay green
- [ ] T015 [US1] Update `bridge/src/reader_blocks.cpp`: `spans_to_html` emits `<a href="l<N>"><u>…</u></a>` for link spans and every text block gets a `links` array of `{url, confirm}` (contracts/qml-bridge.md); T012 MUST pass
- [ ] T016 [US1] Add `Q_INVOKABLE bool openUrl(const QString& url)` to `app/device_bridge.h`/`app/device_bridge.cpp`: re-validate with `pbnotes::is_followable_url`, device (`PLATFORM_POCKETBOOK`): `OpenBook(BROWSER_FOR_AUTH, url.toUtf8().constData(), 0)`, host: `QDesktopServices::openUrl(QUrl(url))`; return false when rejected; log one event with only the result word (no address); make sure `app/CMakeLists.txt` links `notes_core`
- [ ] T017 [US1] Wire link activation in `qml/BlockText.qml` (`Text.onLinkActivated` emits `linkActivated(link)`) and `qml/pages/ReaderPage.qml`: `followLink(block, link)` parses `l<N>`, reads `block.links[N]`; `confirm == false` calls `device.openUrl(url)` at once; `confirm == true` opens the existing `ConfirmDialog` (`qml/ConfirmDialog.qml`) with title "Open this link?", the complete address as text and buttons "Open"/"Cancel"; "Open" calls `device.openUrl(url)`; a `false` result shows "The link could not be opened." in the reader's message area; declining changes nothing and the reading position is untouched
- [ ] T018 [US1] [MANUAL] Device check (quickstart §3 step 1 and probes "Browser return"): record in `research.md` R-8 whether the app is kept alive and shows the same note and position after the browser closes, and what happens with Wi-Fi off; if the position is lost, add a follow-up task to persist note id and `contentY` before `openUrl` (plan risk 3)

**Checkpoint**: links work end to end; the host suite is green; US1 is demonstrable on its own.

---

## Phase 4: User Story 2 - Read tables in notes (Priority: P1)

**Goal**: GitHub-style pipe tables render as a legible grid that fits the screen, with a distinct header,
alignment, wrapping cells, and page breaks that never cut a row (spec FR-007 to FR-013).

**Independent Test**: quickstart §3 step 2 — the test note's table shows a bold header with a rule,
left/center/right columns, a wrapping long cell, working emphasis and link in cells, and "Page down"
never cuts a row; opening the note in the editor shows the original pipe text unchanged.

### Tests for User Story 2 ⚠️ write first, see them fail

- [ ] T019 [P] [US2] Write `tests/unit/test_markdown_table.cpp` for table parsing (research R-10): header + delimiter + body rows; alignment markers `:--`, `:-:`, `--:` and none; optional leading/trailing pipes; escaped pipe `\|` stays in the cell; header/delimiter cell-count mismatch ⇒ plain paragraphs; missing or invalid delimiter ⇒ plain paragraphs (never a partial table); uneven rows (column count = largest row length, header and short rows padded with empty cells, extra cells kept); cell text trimmed and parsed inline (emphasis, link, emoji, empty cell); table ends at a blank line or a line without an unescaped pipe; not recognized inside a list item, a `>` quote, a code fence or when indented 4+ columns; two tables in one note; `Block.line_index` and `line_count` of the table and `line_index` of the block after it; column `weight` values are > 0, sum to 1 and a long cell weighs more than a short one but is clamped; a 500-row table parses in under 50 ms; `toggle_task_item` is unaffected by tables
- [ ] T020 [P] [US2] Write `tests/unit/test_paging.cpp` for the pure paging function (contracts/core-library.md `paging.h`): no unsplittable blocks ⇒ plain step; a block straddling the bottom edge moves the next page start to the block's top; symmetrical for going back (new top moves up to the block's top); a block taller than the view is ignored; the target always makes progress unless at the start or end; clamped to `[0, content_height - view_height]`; adjacent unsplittable blocks (table rows); empty list
- [ ] T021 [US2] Add table cases to `tests/bridge/test_reader_blocks.cpp`: a table becomes one `tablerow` block per row with `header` (true only on the first), `cols` (`{html, links, align}`), `weights`, `unsplittable == true`, `lastRow` (true only on the last row); links in cells carry their own `links`; non-table text around the table keeps its blocks

### Implementation for User Story 2

- [ ] T022 [US2] Extend `core/include/pbnotes/markdown.h` with `BlockKind::Table`, `Block::line_count` and `TableData` (contracts/core-library.md); create `core/src/markdown_table.cpp` (recognition, cell splitting on unescaped pipes, trimming, inline parsing of cells via the shared inline parser, column count, padding, alignment, weights clamped so no column is narrower than a minimum share) and call it from the block loop in `core/src/markdown.cpp`; add to `core/CMakeLists.txt`; T019 MUST pass
- [ ] T023 [US2] Create `core/include/pbnotes/paging.h` and `core/src/paging.cpp`: `struct Interval { double top, bottom; }` and `double page_target(double content_y, double view_height, double content_height, double step, int direction, const std::vector<Interval>& unsplittable)` per T020; add to `core/CMakeLists.txt`; T020 MUST pass
- [ ] T024 [US2] Update `bridge/src/reader_blocks.cpp`: flatten each `Block::Table` into `tablerow` blocks per contracts/qml-bridge.md (`header`, `cols`, `weights`, `unsplittable`, `lastRow`); add `Q_INVOKABLE double pageTarget(double contentY, double viewHeight, double contentHeight, double step, int direction, const QVariantList& unsplittable)` to `bridge/include/notes_bridge.h`/`bridge/src/notes_bridge.cpp` delegating to `page_target`; T021 MUST pass
- [ ] T025 [P] [US2] Create `qml/TableRowView.qml` and add to `qml/qml.qrc`: one row item whose column widths are `max(minWidth, width * weight)` renormalized to the available width (if `n * minWidth` exceeds the width, `minWidth = width / n`), cell `Text` with wrapping, `textFormat: Text.StyledText`, 10 px padding, link activation forwarded like `BlockText.qml`, per-cell alignment, 2 px black cell borders, header bold with a 4 px rule below, closing rule when `lastRow`; the row height is the tallest cell
- [ ] T026 [US2] Update `qml/PageButtons.qml` and `qml/pages/ReaderPage.qml`: `PageButtons` gets `property var unsplittable: []` and calls `bridge.pageTarget(...)` when the list is non-empty (list pages keep their existing code path); `ReaderPage` builds the `[{top, bottom}]` list from the `Repeater` items whose block has `unsplittable` (recomputed when layout changes), renders `tablerow` blocks with `TableRowView`, and the edge tap zones call the same `pageBy`
- [ ] T027 [US2] [MANUAL] Device check (quickstart §3 step 2): header, alignment, wrapping, links and emphasis in cells, paging never cuts a row in portrait and landscape; then SC-002 with ten real tables from the user's notes (2–6 columns, up to 30 rows); record results and any layout fixes in `research.md` R-10

**Checkpoint**: tables work; US1 and US2 together form the P1 slice and can ship before anything else.

---

## Phase 5: User Story 3 - See and keep emojis (Priority: P2)

**Goal**: Emojis survive display, edit, sync and search unchanged, are drawn with a monochrome font where
possible, and are never cut in half (spec FR-014 to FR-018).

**Independent Test**: quickstart §3 step 3 — the test note's emoji line shows in the reader, list and
editor; editing one unrelated word on the device and syncing leaves the server's emoji bytes identical;
searching `😀` finds the note.

### Tests for User Story 3 ⚠️ write first, see them fail

- [ ] T028 [P] [US3] Write `tests/unit/test_utf8_cluster.cpp` for `utf8_cluster_end` (contracts/core-library.md): a single emoji; `❤️` (with variation selector); `👍🏽` (skin tone); `👨‍👩‍👧` (ZWJ family) as one unit; `🇩🇪` as one unit and `🇩🇪🇫🇷` as two; `1️⃣` (keycap); a tag sequence (England flag); `e` + combining acute; plain ASCII and plain multi-byte letters one code point each; invalid UTF-8 bytes count one unit each; at the end of the text returns `size`; the result is always greater than `pos` for `pos < size`
- [ ] T029 [P] [US3] Write `tests/unit/test_emoji_roundtrip.cpp`: JSON parsing of `"😀"` and of the raw UTF-8 `😀` give identical bytes; a lone surrogate still fails as before; JSON writing keeps 4-byte characters as raw UTF-8 and re-parses equal; `NoteStore` `create_local`, `save_local`, `toggle_task`, `set_favorite` and `set_category` keep every emoji byte of the content (include a ZWJ family, a flag and a skin-tone emoji); the derived title keeps an emoji; searching (`ListQuery.search`) for `😀` and for `🇩🇪` finds the note and `fold_case` leaves emojis unchanged; a push-then-pull sync through `tests/support/fake_notes_server.h` returns the content byte for byte, including when the fake server escapes non-ASCII as `\uXXXX`
- [ ] T030 [US3] Extend `tests/unit/test_title.cpp` for `make_excerpt` with emojis at the cut boundary (each kind from T028): the excerpt never ends inside a unit, may exceed `max_chars` by at most one unit, and is unchanged for text without emojis (all existing cases stay)
- [ ] T031 [US3] Add emoji cases to `tests/bridge/test_notes_bridge.cpp`: `openNote` html and `listNotes` entries keep emojis intact (only `<`, `>`, `&` escaped), the displayed title keeps a leading emoji, a note whose first line is only an emoji is listed

### Implementation for User Story 3

- [ ] T032 [US3] Create `core/include/pbnotes/utf8.h` and `core/src/utf8.cpp` implementing `utf8_cluster_end` per T028; add to `core/CMakeLists.txt`; T028 MUST pass
- [ ] T033 [US3] Update `make_excerpt` in `core/src/title.cpp` to stop only at unit boundaries using `utf8_cluster_end`; T030 MUST pass; grep `core/` for other byte-count cuts of user text (`substr` on content, titles, categories) and either fix them with the same helper or record in the task notes why they cannot split a unit
- [ ] T034 [US3] Load the emoji font in `app/main.cpp`: resolve the file (device: `/mnt/ext1/applications/pbnotes-emoji.ttf`; host: the path in the compile definition `PBNOTES_EMOJI_FONT` pointing at `third_party/noto-emoji/NotoEmoji.ttf`, overridable by the environment variable `PBNOTES_EMOJI_FONT`), `QFontDatabase::addApplicationFont`, and on success set the application font with `setFamilies({<current family>, <emoji family>})`; on failure log one event `font.emoji_missing` and continue with the device font (no crash, no dialog); add the compile definition in `app/CMakeLists.txt`
- [ ] T035 [P] [US3] Audit `qml/` (`grep -rn "font.family\|font.families" qml/`) and remove any per-item font family override so every `Text`, `TextField` and `TextArea` inherits the application font; add a one-line comment in `qml/Main.qml` stating that the application font carries the emoji fallback
- [ ] T036 [US3] [MANUAL] Device check (quickstart §3 step 3 and probe "Emoji font"): glyphs in reader, list and editor; which sequences (flag, ZWJ family, skin tone, keycap) draw as one symbol and which show as parts; whether the missing-glyph box for an unsupported emoji is visible; caret movement and line wrapping never split a unit; search for 😀; compare the server text of the emoji line byte for byte after an edit-and-sync of an unrelated word (SC-003). Record in `research.md` R-12. If the missing-glyph box is invisible, add a follow-up task for a display-only replacement string (never the stored text)

**Checkpoint**: emojis preserved and drawn; independent of US1/US2.

---

## Phase 6: User Story 4 - View attachments in a note (Priority: P2)

**Goal**: Attachments appear in place as placeholders with name and (when known) size; the user
downloads them on request; downloaded images show in place and full screen; other files open with the
device viewer; everything downloaded works offline; sync never downloads anything (spec FR-019 to
FR-024, FR-026, FR-031).

**Independent Test**: quickstart §3 step 4 — after the first sync nothing is downloaded; Download the
PNG and the PDF; with Wi-Fi off they still open; the over-limit file is refused with a message.

### Tests for User Story 4 ⚠️ write first, see them fail

- [ ] T037 [P] [US4] Write `tests/unit/test_attachment_ref.cpp` for `attachment_ref.h` (contracts/core-library.md; data-model.md "Reference validation"): `validate_attachment_path` rejects an empty path, more than 1,024 characters, NUL or control characters or a backslash after percent-decoding each segment, a scheme (`^[A-Za-z][A-Za-z0-9+.-]*:`, except the local token scheme `pbatt:`), a leading `/`, and an empty last segment; accepts `..` segments and `.attachments.12/photo one.png`; `encode_attachment_path` percent-encodes each segment (spaces, `(`, `)`, non-ASCII) keeping `/` and round-trips with decoding; `attachment_kind_for` is `Image` for jpg, jpeg, png, gif, bmp, webp (case-insensitive) and `File` otherwise, including no extension; `safe_extension` returns the lower-case extension matching `[a-z0-9]{1,8}` (`.tar.gz` ⇒ `gz`) or an empty string for anything else; `is_viewable_extension` is true exactly for pdf, epub, fb2, txt, rtf, doc, docx, html, htm, djvu, mobi, cbz, cbr; `make_token`, `find_tokens`, `replace_token`, `strip_tokens` (a token alone on its line removes the line break too; mid-line keeps the surrounding text; unknown ids leave the text unchanged; several tokens)
- [ ] T038 [P] [US4] Write `tests/unit/test_markdown_attachments.cpp`: `![name](.attachments.12/a%20b.png)` ⇒ `BlockKind::Attachment` with the decoded validated `path`, `name` and `is_image_syntax`; link syntax `[f](.attachments.12/f.pdf)` ⇒ attachment; a relative link that does not start with `.attachments.` ⇒ plain label (not an attachment); absolute `/x.png` and scheme targets are not attachments; `pbatt:7` ⇒ attachment block with `token_id == 7`; a reference in the middle of a sentence splits the line into paragraph, attachment, paragraph with the same `line_index`; two references on one line; a reference inside a code fence stays literal; inside a list item the item text stays a list item and the attachment block follows it; inside a table cell the attachment is a plain-text span with its name (depends on the table parser of US2)
- [ ] T039 [P] [US4] Write `tests/unit/test_store_migration_v2.cpp`: a schema-1 database (build it with the MVP schema SQL in the test) opens as schema 2 with notes, account and settings intact and an empty `attachments` table; the pre-migration backup is made and deleted after the integrity check; `after_backup_hook` returning false leaves the original database untouched and reports an error; a schema-3 database is refused; ON DELETE CASCADE removes attachment rows when a note is removed via `remove_note`, a pull deletion (`apply_remote`) and `reset_local_data`
- [ ] T040 [P] [US4] Write `tests/unit/test_store_attachments.cpp` for the download-side store operations (data-model.md, constraints quoted): `record_downloaded` and `record_size` create/update a row keyed by note and `remote_path` (unique); `size` NULL = unknown and, when set for a downloaded/pending/staged row, MUST be ≤ 10 MiB (10,485,760) or the call is refused; `display_name` is the last path segment with control characters removed and at most 255 characters; `kind` is `image` if the extension is jpg, jpeg, png, gif, bmp or webp (case-insensitive), else `file`; `local_file` is `<id>.<ext>` with ext the lower-case original extension matching `[a-z0-9]{1,8}` or none; `state` is one of staged, pending, remote, downloaded; `set_attachment_error` accepts only `''`, `too_large`, `not_found`, `rejected`, `unsupported_server`, `storage_full`, `unreadable`, `format_unsupported`, `network`; `attachment_for` finds by note and path; `referenced_local_files` lists the files of downloaded/pending rows; `purge_unreferenced_downloads` deletes `downloaded` rows whose path is no longer referenced by the note's text and keeps `pending`/`staged`/`remote` rows; `apply_remote` calls it for notes whose content changed
- [ ] T041 [P] [US4] Write `tests/contract/test_api_attachments.cpp` plus fixtures `tests/fixtures/attachment_404.unverified.json`, `attachment_400_old_server.unverified.json`, `attachment_head_ok.unverified.json` (anonymized per `tests/fixtures/README.md`, shapes from T005): the request URL is `<server>/index.php/apps/notes/api/v1.4/attachment/<id>?path=<encoded>` with Basic credentials and no `If-Match`; download success stores the body at the `.part` path; 404 ⇒ `not_found`; 400 ⇒ `unsupported`; 401/403 ⇒ Auth abort class; 503 ⇒ Maintenance; a `Content-Length` over the limit ⇒ `too_large` and no file; a transport-cut body at the limit ⇒ `too_large` and no file; `probe_attachment` returns the length from `HEAD`, no size when absent, `not_found` on 404 and honors the 3 s timeout setting; `supports_attachments("1.0, 1.3, 1.4")` is true and `("1.0, 1.3")` false
- [ ] T042 [P] [US4] Write `tests/integration/test_attachment_transport.cpp` using `tests/support/tls_test_server.h`: the real `CurlTransport` streams a download to `save_body_to`, aborts at `max_body_bytes` by `Content-Length` and by running total (chunked body) leaving no file, performs `HEAD` without a body, refuses a certificate failure without sending credentials, and does not forward credentials to a redirect target on another host (extend the pattern of `tests/integration/test_redirect_credentials.cpp`)
- [ ] T043 [P] [US4] Write `tests/integration/test_sync_attachments.cpp` (download side) with a fake server that counts attachment requests: a sync of notes whose text references attachments makes ZERO attachment requests; `AttachmentService::download` stores the file under `<id>.<ext>` and marks the row `downloaded`; 404 ⇒ `not_found` and the placeholder row stays; too large ⇒ nothing stored; simulated full storage ⇒ `storage_full`, the `.part` file removed, earlier files intact; `probe_size` caches the size and returns nothing when `HEAD` gives none; `purge_files()` deletes files no row names and every `*.part`; a pull that removes the reference deletes the downloaded row and (after `purge_files`) its file; a deleted note's attachments disappear
- [ ] T044 [US4] Add attachment cases to `tests/bridge/test_reader_blocks.cpp` and create `tests/bridge/test_attachment_image_provider.cpp` (offscreen Qt): blocks of kind `attachment` have `attState` `placeholder` (no row), `ready` (downloaded) or `error` (row with `error_key`, plain-sentence `errorText`), `size` of -1 when unknown, `imageSource == image://attachment/<id>` only for a ready image, `remotePath` set, `viewable` true only for a ready file with a viewable extension; the provider returns an 8-bit grayscale image no wider than the requested `w` for a generated PNG, keeps the aspect ratio, and a null image for a corrupt file; `formatSize` returns human-readable sizes ("1.0 MB")

### Implementation for User Story 4

- [ ] T045 [US4] Extend `core/include/pbnotes/types.h` and `core/src/types.cpp` with `AttachmentKind`, `AttachmentState`, `Attachment`, `kMaxAttachmentBytes = 10 * 1024 * 1024`, `SyncCounts::attachments_uploaded` and `attachments_failed`, and string conversions for kinds/states (contracts/core-library.md)
- [ ] T046 [US4] Create `core/include/pbnotes/attachment_ref.h` and `core/src/attachment_ref.cpp` implementing everything in T037; add to `core/CMakeLists.txt`; T037 MUST pass
- [ ] T047 [US4] Extend the Markdown parser for attachments: `BlockKind::Attachment` and `AttachmentRef` in `core/include/pbnotes/markdown.h`; reference detection and line splitting in `core/src/markdown_inline.cpp` and `core/src/markdown.cpp`; attachments inside table cells become name-only plain spans in `core/src/markdown_table.cpp`; T038 MUST pass (do this after T022)
- [ ] T048 [US4] Extend the transport seam: `Request::save_body_to`, `Request::max_body_bytes`, `Response::body_too_large` and method `HEAD` in `core/include/pbnotes/http_transport.h`; make `tests/support/fake_transport.h` and `tests/support/fake_notes_server.h` honor them and answer `GET`/`HEAD` on the attachment route (configurable files, sizes, missing files, request counters); document the fields in contracts/core-library.md if names changed
- [ ] T049 [US4] Implement `HEAD` (`CURLOPT_NOBODY`), the file sink (write callback to `save_body_to`), and the size cap (check `Content-Length` in a header callback and count bytes; abort and delete the part file) in `core/src/curl_transport.cpp`, sharing the existing TLS, protocol and redirect options unchanged; T042 MUST pass
- [ ] T050 [US4] Add `supports_attachments`, `download_attachment` and `probe_attachment` (with the `v1.4` URL builder using `encode_attachment_path`, query encoding, 3 s timeout for probe, error mapping per contracts/notes-attachments-api.md) to `core/include/pbnotes/api_client.h` and `core/src/api_client.cpp`; T041 MUST pass
- [ ] T051 [US4] Implement schema version 2 in `core/src/note_store.cpp` (`kSchemaVersion = 2`, the `attachments` table, `attachments_note` index and the partial unique index `attachments_path` exactly as in data-model.md, migration through the existing backup path) and the download-side operations in new `core/src/note_store_attachments.cpp` (`get_attachment`, `attachment_for`, `record_downloaded`, `record_size`, `set_attachment_error`, `referenced_local_files`, `purge_unreferenced_downloads`) declared in `core/include/pbnotes/note_store.h`; call `purge_unreferenced_downloads` from `apply_remote` for notes whose content changed; add the file to `core/CMakeLists.txt`; T039 and T040 MUST pass
- [ ] T052 [US4] Create `core/include/pbnotes/attachment_service.h` and `core/src/attachment_service.cpp` with `download(note_local_id, remote_path)` (download to `<id>.part`, rename, `record_downloaded`; map `ENOSPC` to `storage_full` and clean up), `probe_size(note_local_id, remote_path)` (HEAD, `record_size`) and `purge_files()` (the janitor of data-model.md); the constructor takes the store, a transport and the attachment directory; T043 MUST pass
- [ ] T053 [US4] Extend `bridge/src/reader_blocks.cpp` for `attachment` blocks per contracts/qml-bridge.md (look up the row with `attachment_for`, states `placeholder`/`ready`/`error`, `size`, `errorText` mapped from `error_key`, `imageSource`, `viewable`, `unsplittable`); add `formatSize` to `bridge/src/notes_bridge.cpp`; pass the attachment directory in; T044 (blocks, formatSize) MUST pass
- [ ] T054 [P] [US4] Create `bridge/include/attachment_image_provider.h` and `bridge/src/attachment_image_provider.cpp`: a `QQuickAsyncImageProvider` for `image://attachment/<id>?w=<pixels>` that resolves the row's local file through the store, decodes with `QImageReader` (scaled decoding, `setAutoTransform(true)`), keeps the aspect ratio at the requested width, converts to `Grayscale8`, and returns a null image for unreadable or unsupported files; T044 (provider) MUST pass
- [ ] T055 [US4] Create `bridge/include/attachments_bridge.h` and `bridge/src/attachments_bridge.cpp` (`AttachmentsBridge`, contracts/qml-bridge.md): `supported`, `maxBytes`, `download(noteId, remotePath)` and `probeSizes(noteId)` on a worker thread with the same lifetime pattern as `syncWorker_` in `bridge/src/notes_bridge.cpp` (join in the destructor, one operation at a time per attachment), reuse the transport and CA construction `NotesBridge` uses for sync (extract a shared helper if it is private), skip `probeSizes` when `device.wifiConnected()` is false, signals `attachmentsChanged(noteId)` and `attachmentFailed(noteId, remotePath, text)`, `openAttachment(attachmentId)` calling `device.openFile`; add to `bridge/CMakeLists.txt`
- [ ] T056 [US4] Add `Q_INVOKABLE bool openFile(const QString& path)` to `app/device_bridge.h`/`app/device_bridge.cpp`: the path must be inside the attachment directory and `is_viewable_extension`; device: `OpenBook(path, "", 0)`, host: `QDesktopServices::openUrl(QUrl::fromLocalFile(path))`; false otherwise; log the result word only
- [ ] T057 [US4] Update `app/main.cpp`: create `<data dir>/attachments/`, construct `AttachmentService`/`AttachmentsBridge`, register the `attachments` context property and the `attachment` image provider, call `purge_files()` at start, and log one event listing `QImageReader::supportedImageFormats()` (format names only); make `NotesBridge` run `purge_files()` after a sync finishes
- [ ] T058 [P] [US4] Create `qml/AttachmentView.qml` (add to `qml/qml.qrc`): `placeholder` shows the name, the size or nothing when unknown, and a "Download" button (disabled with a "No connection" note when `!device.wifiConnected()`); `ready` image shows `Image { source: imageSource + "?w=" + width; asynchronous: true }` and a tap pushes `ImagePage`; `ready` file shows the entry with name and size and an "Open" button when `viewable` (else "No viewer for this file on this device."); `error` shows the sentence from `errorText` and keeps "Download" where retrying makes sense; marked `unsplittable`
- [ ] T059 [P] [US4] Create `qml/pages/ImagePage.qml` (add to `qml/qml.qrc`): the image at `image://attachment/<id>?w=<window width>` with `fillMode: Image.PreserveAspectFit` filling the window, a Back `InkButton` top-left, no zoom, no pan, no animation
- [ ] T060 [US4] Update `qml/pages/ReaderPage.qml`: render `attachment` blocks with `AttachmentView`; on `attachments.attachmentsChanged(noteId)` reload while preserving `flick.contentY` (pattern already used after `toggleTask`); show `attachments.attachmentFailed` text in the message area; call `attachments.probeSizes(noteId)` once after the note loads; add the attachment blocks' `{top, bottom}` to the `unsplittable` list from T026; going Back from `ImagePage` returns to the same position
- [ ] T061 [US4] [MANUAL] Device check (quickstart §3 step 4 and probes "Image formats", "Viewer hand-off"): nothing downloaded after the first sync (attachment folder empty); placeholders; Download PNG then view in place and full screen; Download PDF then Open hands it to the reader; Wi-Fi off keeps both working; the over-limit file is refused; unsupported image format falls back to the file entry. Record in `research.md` R-7 and R-8 (and decide on the decoder contingency only if JPEG fails)

**Checkpoint**: viewing attachments works; sync is untouched by attachments; US4 demonstrable on its own.

---

## Phase 7: User Story 5 - Add an attachment to a note (Priority: P3)

**Goal**: From the editor, pick a file on the device and attach it, offline; the file reaches the server
before any reference to it, interruptions lose nothing, conflict copies keep their attachments (spec
FR-025, FR-027 to FR-030).

**Independent Test**: quickstart §3 steps 5–7 — offline add shows the note pending; after sync the file
and text are on the server and visible in the web app; an interrupted sync resumes; a conflict copy
keeps its attachment.

### Tests for User Story 5 ⚠️ write first, see them fail

- [ ] T062 [P] [US5] Write `tests/unit/test_store_attachments_staging.cpp`: `stage_attachment` creates a `staged` row with `note_local_id` NULL; `create_local` and `save_local` bind the staged rows whose `pbatt:<id>` token is in the saved content to the note IN THE SAME TRANSACTION as the save (a forced write failure via `test_fail_nth_write` leaves both the note and the rows unchanged) and set them `pending`; tokens with unknown ids or ids already bound to another note are ignored; `discard_staged` drops rows with NULL note; the start-up cleanup (`discard_staged` with the safety check) binds instead of dropping a staged row whose token appears in some note's content; `pending_attachments` lists `pending` rows; `record_uploaded` in ONE transaction sets `remote_path`, `size`, state `downloaded`, replaces the token in the note content with the percent-encoded server path, recomputes title and excerpt, and leaves `modified`, `status` and `etag` untouched, is idempotent, and rolls back completely on a forced write failure; `resolve_conflict` re-binds pending attachments to the conflict copy, duplicates a downloaded attachment for the copy as a new pending row with the token substituted in the copy's text, leaves remote-only references as paths, and rolls back with the copy on failure (existing `tests/unit/test_conflict_atomic.cpp` pattern)
- [ ] T063 [P] [US5] Write `tests/unit/test_attachment_service_add.cpp`: `AttachmentService::add` copies the file to `<id>.part` then renames to `<id>.<ext>`, returns the token, display name and size, never modifies the source (compare content hash and modification time before and after); a file over 10 MiB (10,485,760) ⇒ `too_large`, nothing staged, no `.part`; a missing file, a directory or an unreadable file ⇒ `unreadable`; adding the same file twice ⇒ two distinct rows; two files with the same name from different folders ⇒ distinct; simulated full storage ⇒ `storage_full` with the `.part` removed; a hostile file name (control characters, 400 characters, no extension) is sanitized and never used as a path
- [ ] T064 [P] [US5] Write `tests/contract/test_api_attachment_upload.cpp` plus fixtures `attachment_upload_ok.unverified.json`, `attachment_upload_renamed.unverified.json`, `attachment_upload_413.unverified.json` (anonymized): the request is `POST …/api/v1.4/attachment/<id>` multipart with the single field `file` carrying the display name as file name and no other fields; success returns the validated `remote_path`; a renamed (deduplicated) answer is accepted as returned; an invalid `filename` (absolute, control character, over 1,024 characters, not JSON) ⇒ `rejected`; 413 ⇒ `too_large`; 400 ⇒ `unsupported`; 401/403 and 503 map as for notes; a file over the limit or missing makes no request
- [ ] T065 [P] [US5] Write `tests/integration/test_attachment_upload_transport.cpp` with `tests/support/tls_test_server.h`: the real `CurlTransport` sends a multipart upload whose bytes and file name arrive intact, refuses a certificate failure without sending credentials, and does not forward credentials to a redirect target on another host
- [ ] T066 [US5] Write `tests/integration/test_sync_attachments_push.cpp` with the fake server: (a) existing note with a pending attachment ⇒ request order POST attachment, then PUT with `If-Match` and the rewritten path; local text rewritten; note `void`; counts; (b) new note with attachments ⇒ POST note WITHOUT tokens, then upload, then PUT with the ETag from the POST; the server's final text has the path; (c) a note consisting only of a token ⇒ created with empty content (or the fallback line from T005); (d) upload rejected (413) ⇒ PUT sent with that token stripped from the OUTGOING text only, the local text keeps the token, the attachment keeps `too_large`, outcome `Partial` with message key `attachments_failed`; (e) transport failure during an upload aborts the run, nothing lost, the next run completes without creating the note twice; (f) failure between upload and PUT ⇒ the next run does not re-upload and sends the PUT; (g) a lost upload response ⇒ the next run uploads again and uses the returned name; (h) 412 on PUT after uploads ⇒ server text stays, the conflict copy carries the attachments and uploads them under its own new note id; (i) a server without `1.4` ⇒ attachments stay pending with `unsupported_server`, note text pushed with tokens stripped; (j) deleted-on-server vs edited-on-device with a pending attachment keeps the note and the attachment
- [ ] T067 [US5] Write `tests/bridge/test_attachments_bridge.cpp`: `stageAttachment` runs off the UI thread and emits `stageFinished` with `{ok, id, token, name, size}` or an `errorKey`/`errorText`; `listDirectory` lists directories first, then files, case-insensitive, skips hidden and unreadable entries, returns an empty list for an unreadable path; `NotesBridge::saveNote` with a staged token makes the row `pending`; `discardStaged` drops unsaved staging; `loadForEdit` returns the text with the visible token; `supported` follows the stored API versions; reader blocks show a pending token as `attState == "pending"` with a local `imageSource` for images

### Implementation for User Story 5

- [ ] T068 [US5] Implement the staging and upload-side store operations in `core/src/note_store_attachments.cpp` and `core/include/pbnotes/note_store.h` (`stage_attachment`, `discard_staged`, `pending_attachments`, `record_uploaded`; binding of staged tokens inside `create_local`/`save_local` in `core/src/note_store.cpp`; the start-up safety check); T062 (all but the conflict cases) MUST pass
- [ ] T069 [US5] Extend `resolve_conflict` in `core/src/note_store.cpp` with the re-binding/duplication rules of research R-4 inside its existing transaction; the conflict cases of T062 MUST pass
- [ ] T070 [US5] Implement `AttachmentService::add` in `core/src/attachment_service.cpp` (copy to `.part`, size check, rename, `stage_attachment`, error mapping, `ENOSPC` ⇒ `storage_full`); T063 MUST pass
- [ ] T071 [US5] Implement multipart upload in `core/src/curl_transport.cpp` with `curl_mime` (`Request::upload_file` path, field name `file`, file name) and extend `core/include/pbnotes/http_transport.h`, `tests/support/fake_transport.h` and `tests/support/fake_notes_server.h` (record uploaded parts; answer `POST` on the attachment route with configurable filename, status and dedup); T065 MUST pass
- [ ] T072 [US5] Add `upload_attachment` to `core/include/pbnotes/api_client.h` and `core/src/api_client.cpp` (refuse a missing file or one over 10 MiB before any request; validate the answer's `filename` with `validate_attachment_path`; error mapping per contracts/notes-attachments-api.md); T064 MUST pass
- [ ] T073 [US5] Implement the attachment-aware push in new `core/src/sync_attachments.cpp` (declared in `core/include/pbnotes/sync_engine.h`/private header) and call it from `core/src/sync_engine.cpp` as research R-3 describes: create-without-tokens when the note has no server id, upload each pending attachment and `record_uploaded`, then PUT with `If-Match`, stripping tokens that could not be uploaded from the outgoing text only; count uploads/failures and return `Partial` with message key `attachments_failed`; transport-level failures abort the run as for notes; never downloads; T066 MUST pass
- [ ] T074 [US5] Extend `bridge/src/attachments_bridge.cpp`: `stageAttachment(path)` on a worker thread emitting `stageFinished`, `discardStaged()`, `listDirectory(path)` and `rootDirectory` (device `/mnt/ext1`, host the home directory), `supported` from the account's stored API versions; add the message text for `attachments_failed` ("Some attachments could not be uploaded.") to `sync_message_text` in `bridge/src/notes_bridge.cpp` and show the attachment counts in the status line data; extend `reader_blocks.cpp` for pending tokens (state `pending`, local `imageSource` for images); T067 MUST pass
- [ ] T075 [P] [US5] Create `qml/pages/FilePickerPage.qml` (add to `qml/qml.qrc`): path line, an "Up" control, large rows from `attachments.listDirectory(path)` (directory ⇒ open, file ⇒ confirm dialog showing name and size), `PageButtons` with `snap` for whole rows, files larger than `attachments.maxBytes` refused with a message before the dialog; emits the chosen path to the editor
- [ ] T076 [US5] Update `qml/pages/EditorPage.qml`: an "Attach" button (disabled with a note when `!attachments.supported`) that pushes `FilePickerPage`, calls `attachments.stageAttachment(path)`, and on `stageFinished` inserts the token with `area.insert(...)` at the start of the cursor's line followed by a line break; `dirty` includes the inserted text so the existing discard prompt applies; leaving the editor (save or discard) calls `attachments.discardStaged()`; an error from staging is shown in the existing message area
- [ ] T077 [US5] [MANUAL] Device check (quickstart §3 steps 5–7): offline add shows the note pending and the file as waiting to upload; Discard leaves no staged file in the attachment folder; sync uploads, the web app renders the image, the local text now holds the server path; an interruption (Wi-Fi off midway, app killed) resumes without loss; a conflict copy keeps the attachment. Record results in `research.md` R-3/R-4 and the lost-response behavior if it can be provoked

**Checkpoint**: all five stories work; the full quickstart can be run.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [ ] T078 [P] Update `README.md` and `INSTALL.md`: new capabilities (links, tables, emojis, attachments), the 10 MiB attachment limit, that attachments download on request, the bundled emoji font and its license (`applications/pbnotes-emoji-OFL.txt`), and that the schema upgrade is one way (an older build refuses the upgraded database)
- [ ] T079 [P] Extend `tests/unit/test_perf_read.cpp`: opening a note with a 200-row table, 50 links and 100 emojis builds its reader blocks in well under the SC-005 budget on the host, and the 1,000-note collection benchmark still meets the MVP read and search limits (SC-003 and SC-008 of the MVP spec)
- [ ] T080 [P] Re-read `contracts/core-library.md`, `contracts/qml-bridge.md`, `data-model.md` and the `plan.md` structure list against the code as built (they already describe `paging.h`, `bridge.pageTarget` and staged tokens bound inside `create_local`/`save_local`) and fix every name, field or signature that changed during implementation (especially from T048, T071, T073); keep `research.md` R-15 results current
- [ ] T081 Run `scripts/pre-merge.sh` (tests, `check-constitution.sh` including the new logging rule, fixture anonymization, secret scan) and fix every finding; confirm no log call in the new code carries a URL, name or path
- [ ] T082 Security pass over the new code (`/security-review`): reference validation and percent-decoding, URL validation before `OpenBook`, no local path derived from remote strings, file permissions of the attachment folder, credentials never sent on redirects for attachment URLs, handling of oversized or hostile files and tokens
- [ ] T083 [MANUAL] Run the whole quickstart on the device (§3 all steps, §5 spot checks SC-001 to SC-009) and record the outcomes; update `specs/002-rich-note-content/checklists/requirements.md` notes and mark remaining open items

---

## Dependencies & Execution Order

- **Phase 1** (Setup): T001–T004 and T006 can run in parallel; T003 needs T001; T005 (manual) should finish before T041/T064 fixtures are finalized and before T073.
- **Phase 2** (Foundational): T007 → T008 → T009 (T008 and T009 are independent of each other after T007's CMake change; run in order to keep the tree green). Blocks every story.
- **US1** (P1): T010–T012 tests first; T013 → T014 → T015; T016 independent of T013–T015 except the shared validator (needs T013); T017 needs T009, T015, T016; T018 manual.
- **US2** (P1): T019–T021 tests first; T022 → T024 (needs T023 for `pageTarget`); T025 parallel with T024; T026 needs T024, T025, T009; T027 manual. Starts after US1's T017 if they share one working tree (both edit `ReaderPage.qml`).
- **US3** (P2): independent of US1/US2 (core, title, font loading); T028–T031 tests first; T032 → T033; T034 and T035 parallel; T036 manual.
- **US4** (P2): needs Phase 2; T047 additionally needs T022 (tables) for the cell rule; T037–T044 tests first; T045 → T046/T047 → T048 → T049/T050 → T051 → T052 → T053/T054 → T055 → T056/T057 → T058/T059 → T060 → T061.
- **US5** (P3): needs all of US4; T062–T067 tests first; T068 → T069 → T070; T071 → T072 → T073; T074 → T075/T076 → T077.
- **Phase 8**: after all chosen stories.

## Parallel Opportunities

- Phase 1: T001, T002, T004, T006 together; then T003.
- US1 tests: T010 and T011 together. US2 tests: T019 and T020 together; T025 beside T024. US3: T028 and T029 together; T034 beside T035.
- US4 tests: T037, T038, T039, T040, T041, T042, T043 are all different files and can be written together; T054, T058, T059 are independent files.
- US5 tests: T062, T063, T064, T065 together; T075 beside T074.
- US3 can proceed in parallel with US1 or US2 by another person or worktree (no shared files except `app/main.cpp` in T034 vs T057 — sequence those two).

## Implementation Strategy

1. **MVP slice**: Phase 1 (T001–T004, T006) → Phase 2 → **US1** → stop and validate on the device (T018). This delivers links, the most requested item, with no storage or sync change.
2. **P1 slice**: add **US2** (tables), validate (T027). US1 + US2 can be released together.
3. **P2**: **US3** (emojis) is independent and low risk; do it any time after Phase 2. **US4** (view attachments) is the largest piece; finish it before US5.
4. **P3**: **US5** (add attachments) last — it carries the data-safety risk, so its tests (T062–T066) come first and T073 is reviewed against the constitution (Principle VI) before merge.
5. Run T081 after every story, not only at the end; keep the host suite green at every commit.
