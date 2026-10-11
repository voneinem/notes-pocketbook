---

description: "Task list for better Markdown support and the slash menu"
---

# Tasks: Better Markdown Support and Slash Menu

**Input**: Design documents from `/specs/006-markdown-slash-menu/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: REQUIRED. The constitution makes Test-First non-negotiable (Principle I): every test task
below MUST be written and observed FAILING before the implementation task(s) it names (a test that
does not compile against the missing header or signature counts as failing). Tests for `notes_core`
run on the host without Qt, the SDK or a device (Principle V). Unit tests under `tests/unit/` are
picked up by the glob in `tests/CMakeLists.txt`; bridge tests live in `tests/bridge/` and are listed
by name in the bridge loop of `tests/CMakeLists.txt` (this feature adds none: `test_edit_bridge` and
`test_reader_blocks` already exist); QML page tests need an `add_test(... test_qml_pages <driver.qml>
<page.qml>)` entry in the QML block of `tests/CMakeLists.txt`. A test that characterizes behavior
which already works (marked *expected to pass*) is still written first; a failure there is a bug to
fix in the parser, not a reason to change the test.

**Organization**: Tasks are grouped by user story. Paths are relative to the repository root. This
feature builds on features 001–005; all existing tests must stay green.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: US1..US5 as in spec.md
- **[MANUAL]**: needs a person or the reader; record the result in the named file. An implementing
  agent skips these and reports them as open

## Conventions that apply to every task

- C++17; namespace `pbnotes` in the core; `notes_core` MUST NOT include Qt or `inkview.h` headers
  (`scripts/check-constitution.sh` R3). Qt types appear only in `bridge/`, `app/` and `qml/`.
- **No text in logs**: no log line, `qDebug`/`qWarning`, file or server field may carry note text,
  typed text, copied text or a title. Event names and counts only (constitution R4/R6).
- **Nothing from a note becomes markup**: the bridge builds `<b>`, `<i>`, `<s>`, `<u>`, `<a href="lN">`
  itself from span flags and escapes every text with `toHtmlEscaped()`; note text such as `<u>` or
  `<details>` that is not recognized as a format stays escaped text (constitution III).
- Limits and rules, quoted from the design (apply in code, not by convention):
  - Fence opener: "at most 3 spaces of indent (a tab counts 4), then three or more of one character,
    `` ` `` or `~`; the info string of a backtick opener must not contain a backtick". Closer: "same
    character, at least as many, only spaces after". An unclosed fence "runs to the end of the note".
  - Quote line: "at most 3 spaces of indent, `>`, optional single space". Info kinds: `[!NOTE]`,
    `[!TIP]`, `[!WARNING]`, `[!IMPORTANT]`, `[!CAUTION]`, "ignoring case and trailing spaces".
  - Details: only at top level (not in a fence, not in a quote, not already in a section); a line that
    is only `<details>` or `<details open>` (case-insensitive, at most 3 spaces of indent, trailing
    spaces ok); `</details>` alone on a line ends it; an unclosed section runs to the end of the note.
  - A line "longer than 16,384 bytes stays plain text" (no quote, details or inline formats); a `Code`
    line is still shown literally in full.
  - Slash menu entries, in this order: `bullet_list`, `table`, `info_box`, `link`, `bold`, `italic`,
    `underline`, `strikethrough`, `details`, `quote`, `code_block`, `emoji`.
  - Trigger: a single "/" typed at the start of the text or directly after `\n`, space or tab; "//" =
    the second "/" directly after the one that opened the menu. Never in the Title field, never for
    pasted, undone or menu-inserted text.
  - Date-time text: `YYYY-MM-DD HH:MM`, zero-padded, 24-hour; "year 1970–2099, month 1–12, day
    1–`days_in_month`, hour 0–23, minute 0–59"; the picker clamps rather than rejects.
  - Emoji grid: "48 single-code-point emojis" (no ZWJ sequences, no skin tones, no variation
    selectors), 6 columns × 8 rows, each cell at least 78 × 78 px.
  - Heading scale `[1.9, 1.55, 1.3, 1.15, 1.1, 1.05]` × body size, bold; code lines at 0.9 × body size.
- UI strings go through `qsTr`. Interfaces are those of `contracts/core-library.md` and
  `contracts/qml-bridge.md`; if a task finds a contract wrong, fix the contract in the same commit.
- Device builds use the manual moc list: a new `Q_OBJECT` header must be added to `pbnotes_moc(...)`
  in `bridge/CMakeLists.txt` (this feature adds a class to an existing header, so no new entry), and
  every new QML file must be added to `qml/qml.qrc`.

---

## Phase 1: Setup

- [X] T001 Build the host target and run the whole suite on the untouched branch
  (`cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build-host && ctest
  --test-dir build-host --output-on-failure`). Every test must pass (including `app_self_test` and
  the `qml_*` tests); if one does not, stop and report it — later failures must be attributable to
  this feature.
- [X] T002 Characterize today's parser before touching it: create `tests/unit/test_markdown_golden.cpp`
  and `tests/fixtures/markdown_golden.txt`. The test holds an inline corpus of at least 30 notes
  that use **none** of the new syntax (no fence, no `>` quote, no `<details>`, no `~~`, no `<u>`):
  headings 1–6 (with and without the space), paragraphs, bullet/numbered/task lists with depths,
  bold/italic/bold-italic and unmatched marks, titled links, bare addresses, autolinks, external
  images, attachment references, tables (aligned, ragged, malformed), blank lines, CRLF text, a line
  of 20,000 bytes, emoji and combining marks. For each note it dumps one line per block (`kind`,
  `line_index`, `level`, `depth`, `ordered`, `number`, `is_task`, `checked`, `src_begin`, `src_end`,
  and for each span `kind`, `text`, `url`, `link_confirm`) and compares with the golden file; with
  `PBNOTES_UPDATE_GOLDEN=1` it rewrites the file instead. Generate the golden file now, with the
  untouched parser, and commit it. The test passes immediately and MUST stay green through every
  later phase (FR-020: nothing outside the new formats changes).

---

## Phase 2: Foundational (the block and span model — used by US1 and US4)

**Purpose**: the fields every later task fills in or reads. Header-only; no behavior changes.

- [X] T003 In `core/include/pbnotes/markdown.h` add exactly the declarations of
  `contracts/core-library.md`: `BlockKind::Fence, Code, Callout, Summary, Marker` (after
  `Attachment`); `enum class QuoteKind { None, Quote, Info }`; `enum class CalloutKind { Note, Tip,
  Warning, Important, Caution }`; `Span::strike` and `Span::underline` (`bool`, default false); on
  `Block`: `quote` (default `None`), `callout` (default `Note`), `quote_first`, `quote_last`,
  `code_first`, `code_last` (default false), `details_id` (default −1), `details_open` (default
  false), `fence_info` (`std::string`). Update the file's header comment (code and the new formats
  are no longer "plain text"). Add `case` labels for the new kinds wherever a `switch` over
  `BlockKind` would now warn (`bridge/src/reader_blocks.cpp` `block_kind()` → `"fence"`, `"code"`,
  `"callout"`, `"summary"`, `"marker"`; `core/src/markdown_copy.cpp` `is_text()` stays as is until
  T013). Build with `-Wall -Wextra`; the whole suite, including T002's golden test, stays green.

**Checkpoint**: model compiles; no behavior changed.

---

## Phase 3: User Story 1 — Read code blocks in notes (Priority: P1) 🎯 MVP

**Goal**: a fenced block (backtick or tilde, optional language such as `yaml`) is shown as one boxed
area in fixed-width type with every line and space as written, literal content, wrapped long lines,
and no line split by a page break; copying returns the fences and code exactly.

**Independent Test**: open a note with a `yaml` block of three nested, indented lines plus `**not
bold**` and `# not a heading` inside it: a distinct box, fixed-width, indentation intact, fence
lines not shown as text, asterisks and hash literal. (quickstart §4 rows 1–3; US1 scenarios 1–10.)

### Tests first (observe failing)

- [X] T004 [P] [US1] Create `tests/unit/test_markdown_fence.cpp` for the fence rule and the code
  blocks (`contracts/core-library.md`; research R-1). Cases (`parse_markdown`): `"```yaml\na:\n  b: 1\n```"`
  → blocks `Fence`(line 0, `fence_info == "yaml"`, `code_first`), `Code` ×2 (lines 1–2, one literal
  span each whose text is the line exactly, `"  b: 1"` keeps its two spaces), `Fence`(line 3,
  `code_last`); `Fence` and `Code` blocks have `src_begin == 0` and `src_end == line length`;
  a `Code` line `**not bold**` → one `Plain` span with the asterisks; `# not a heading` → `Code`, not
  `Heading`; `- item`, `| a | b |`, `[a](https://b.example)` and an attachment reference inside a
  fence → one literal span each, no `url`, no `Attachment` block; `~~~` fence and `~~~~` fence;
  a four-backtick opener is NOT closed by three backticks (the closer needs ≥ 4) but is by five; a
  backtick fence is not closed by `~~~`; closer with trailing spaces closes, with trailing text does
  not (that line is `Code`); opener with 3 spaces of indent is a fence, with 4 spaces or a tab is a
  `Paragraph`; opener `` ```a`b `` (backtick in the info string) is not a fence; a fence with an
  empty body (` ``` ` then ` ``` `) → `Fence`, `Fence` with `code_first` on the first and `code_last`
  on the second and no `Code` between; an unclosed fence → `Fence`, `Code`… to the last line, the last
  `Code` has `code_last`; a fence line directly after a paragraph line needs no blank line; the first
  fence ends before the second starts (two blocks in one note: the `code_first`/`code_last` flags of
  each are independent); a `Code` line of 20,000 bytes is one literal span of all 20,000 bytes (the
  16,384-byte rule does not shorten code); a blank line inside a fence is a `Code` block with an empty
  literal (not `Blank`, so the box stays unbroken) — assert `kind == Code` and empty text;
  `line_index` of every block equals its source line; `find_attachment_refs` and
  `tokenize_attachment_refs` ignore a reference inside a `~~~` fence and inside a fence opened with 3
  spaces of indent but not inside a 4-space-indented "fence". The header functions do not exist yet,
  so the test fails.
- [X] T005 [P] [US1] Extend the existing tests for the shared fence rule (FR-020): in
  `tests/unit/test_markdown.cpp` update the fence expectation near the old line 129 (fence lines are
  now `Fence`, contents `Code`, still one literal span; no emphasis inside) and keep every other
  expectation; in `tests/unit/test_markdown_attachments.cpp` add a reference inside a `~~~` fence (not
  found, not tokenized) and one after the closing fence (found); in
  `tests/unit/test_markdown_toggle.cpp` add: `- [ ] task` inside a fence → `toggle_task_item` returns
  the text unchanged; after the closing fence → toggles. In `tests/unit/test_markdown_table.cpp` add:
  a table row followed directly by a `~~~` line ends the table (like a backtick fence does).
- [X] T006 [P] [US1] Extend `tests/unit/test_copy_source.cpp` and
  `tests/unit/test_markdown_source_map.cpp` (`copy_source`, contract): selecting from the first to the
  last block of a fenced block returns the three-plus-lines source exactly, including both fence
  lines and the language name; a selection that starts in the middle of a `Code` line and ends in
  another returns only the selected characters (no fence, no marks); a selection of a single `Fence`
  block (whole unit) returns that line as written; a paragraph, then a fence block, then a paragraph,
  all selected, returns the three parts joined with `\n` exactly as in the source; a `Code` line with
  `**x**` selected completely returns `**x**` (literal, not interpreted) and partly returns the
  selected characters; an empty `Code` line inside a fence yields an empty line in the copy.
- [X] T007 [P] [US1] Extend `tests/bridge/test_reader_blocks.cpp` (contract `qml-bridge.md`, "Reader
  blocks"): for the T004 yaml note the entries are `kind` `"fence"`, `"code"`, `"code"`, `"fence"`;
  `codeFirst` true on the first, `codeLast` true on the last; every one has `unsplittable == true`;
  the `html` of a `code` entry is the line HTML-escaped with no tags (`a: 1 <b>` → `a: 1 &lt;b&gt;`),
  leading spaces preserved; a `fence` entry has empty `html`; `src` begin/end as in T004; a note
  without fences yields exactly the same entries and keys as before for its blocks (no `codeFirst`
  true anywhere).
- [X] T008 [US1] Create `tests/qml/reader_blocks.qml` (a driver in the style of
  `tests/qml/reader_select.qml`: the real `qml/pages/ReaderPage.qml` with the stubs in
  `tests/qml/stubs.qml`; add a stub note whose `blocks` are the entries of T007) and register it in
  `tests/CMakeLists.txt` next to `qml_reader_select` as `qml_reader_blocks` (same environment and
  label). Checks for this story: one text item per `code` block uses a fixed-pitch font
  (`font.fixedPitch === true`); the code items' pixel size is `Math.round(fontPx * 0.9)`; the three
  blocks of the box together have a visible top edge on the first (`codeFirst`) and bottom edge on the
  last (use `objectName`s `codeTop`/`codeBottom` on the edge rectangles, `visible` accordingly) and
  a left/right border on each; a `fence` block is selected whole by a long press and its `selected`
  outline shows; a long press on a word inside a `code` block selects that word (character range) and
  `copySelection` is asked for the right unit/offsets; the page's `unsplittable` list contains the
  `code` and `fence` rectangles. The checks fail until T015/T016.

### Spike (device) — before T015

- [X] T009 [MANUAL] [US1] Spike S-1 (research R-7): install the current build and show a note with a *(informal: the author tried it on the reader, 2026-10-11, and approved: "works like a charm")*
  fenced block of `iiii` / `MMMM` / indented lines and a paragraph with **bold** beside regular text.
  Record in `specs/006-markdown-slash-menu/checklists/device-results.md`: are code glyphs equal width,
  does indentation line up, is bold visibly heavier than regular? If the code is not fixed-width the
  fallback task T017 becomes mandatory; if bold is not heavier, tell the author before US2.

### Implementation

- [X] T010 [US1] In `core/src/markdown_internal.h` / `core/src/markdown.cpp` add the shared fence
  helper (research R-1): `struct md::FenceOpen { char ch; size_t count; std::string info; };`
  `std::optional<md::FenceOpen> md::open_fence(const std::string& line);` implementing the opener rule
  quoted in *Conventions* (indent counted with a tab = 4; backtick opener rejects a backtick in the
  info string) and `bool md::closes_fence(const std::string& line, const md::FenceOpen& open);`.
- [X] T011 [US1] In `core/src/markdown.cpp` `parse_markdown` replace the `` rest.compare(0, 3, "```") ``
  toggle by `md::open_fence` / `md::closes_fence`: an opener line becomes a `Fence` block (`fence_info`
  set, `src_begin = 0`, `src_end = line.size()`, empty spans); each line until the closer becomes a
  `Code` block with `md::literal_spans(line, 0)` (a blank line stays `Code`, not `Blank`); the closer
  is a `Fence`. After the loop, a post-pass sets `code_first` on the opener (or on the first `Code`
  block when there is no opener in view — not possible) and `code_last` on the closer, or on the last
  `Code` block of an unclosed fence (or on the opener of an unclosed empty fence). Lines longer than
  16,384 bytes inside a fence are still literal (no length shortening). Make T004 pass; T002 stays green.
- [X] T012 [US1] Use the helper at the other three sites: `find_attachment_refs` and
  `tokenize_attachment_refs` in `core/src/markdown.cpp` (replace both `compare(first, 3, "```")`
  toggles; a reference inside a fence is skipped, tilde fences included) and `ends_table` in
  `core/src/markdown_table.cpp` (a line that `open_fence` accepts ends a table; the existing `>` and
  heading tests stay). Make T005 pass.
- [X] T013 [US1] In `core/src/markdown_copy.cpp` treat `Fence` like an attachment piece with
  `begin = 0`, `end = line.size()` (the whole line as written) and `Code` like a text block
  (`is_text()` includes `Code`; `displayed_length` and `text_piece` already work from the literal
  span). Make T006 pass.
- [X] T014 [US1] In `bridge/src/reader_blocks.cpp` emit the contract's fields for `fence` and `code`
  entries: `codeFirst`, `codeLast`, `unsplittable: true`, `html` (code: `spans_to_html`, which escapes;
  fence: empty), plus the existing `block`, `row`, `src`, `line`. Keep every key of the existing
  entries. Make T007 pass.
- [X] T015 [US1] In `qml/BlockText.qml` draw the code box (contract `qml-bridge.md`): for `kind ===
  "code"` the text item uses `font.family: "monospace"`, `font.styleHint: Font.Monospace`,
  `font.fixedPitch: true`, `font.pixelSize: Math.round(fontPx * 0.9)`, wraps within the box, and sits
  inside a rectangle with `color: "#f2f2f2"` and a 3 px `#000000` border on the left and right; for
  `codeFirst` a top edge rectangle (`objectName: "codeTop"`), for `codeLast` a bottom edge
  (`objectName: "codeBottom"`); a `kind === "fence"` block is a 14 px strip carrying that edge (the
  opening fence draws the top edge and sides, the closing one the bottom edge and sides); the block's
  gap to the next block is 0 inside a box (so the lines touch) and 10 px after `codeLast`. Make the
  drawing parts of T008 pass.
- [X] T016 [US1] In `qml/pages/ReaderPage.qml`: `unitIsText(u)` is false for `"fence"` (selected whole
  like a blank line; its delegate gets the whole-unit long-press `MouseArea` already used for table
  rows and attachments); `refreshUnsplittable` already lists blocks with `unsplittable` — confirm the
  `code` and `fence` blocks appear. Make T008 pass.
- [X] T017 [US1] **Only if T009 says the code is not fixed-width** (complexity C-3): bundle one small *(not needed: the author saw the code blocks fixed-width on the reader)*
  OFL monospace font. Add the font file to `res/` (name `pbnotes-mono.ttf`), load it in `app/main.cpp`
  with `QFontDatabase::addApplicationFont` like the emoji font (path
  `/mnt/ext1/applications/pbnotes-mono.ttf` on the device, `PBNOTES_MONO_FONT` define on the host; a
  missing file logs `font.mono_missing` and is not an error), set `font.family` of the code text to
  the loaded family, package it in `scripts/package-device.sh` next to the emoji font, and add its
  license text to `licenses/` and an entry to `NOTICE` and `licenses/manifest.txt`
  (`tests/unit/test_license_consistency.cpp` and `test_about_manifest.cpp` must stay green). If T009
  passes, mark this task "not needed" with the T009 result.
- [X] T018 [US1] Run the whole suite (T002's golden test included). Walk quickstart §3 rows for US1.

**Checkpoint**: User Story 1 works on the host; device check is T009 plus the walkthrough (T069).

---

## Phase 4: User Story 2 — Clearly visible headings and bold text (Priority: P1)

**Goal**: `#`..`######` headings in distinct sizes (levels 1–3 clearly different, 4–6 still headings),
bold clearly heavier than body text everywhere it can occur, a stray `**` shown as typed, and a
heading never the last item on a page.

**Independent Test**: a note with `#`, `##`, `###`, `####` headings, bold in a paragraph, a heading, a
list item and a table cell, and a stray `**`: four sizes in descending order, bold distinguishable,
asterisks as typed. (quickstart §4 rows 4–5; US2 scenarios 1–8.)

### Tests first (observe failing)

- [X] T019 [P] [US2] Create `tests/unit/test_markdown_blocks.cpp` (this task: headings and bold;
  later phases extend the file). *Expected to pass* (the parser already does this; the test pins it):
  `# A`, `## B`, `### C`, `#### D`, `##### E`, `###### F` → `Heading` with `level` 1..6 and the text
  without hashes; `#hashtag` and `####### x` (seven hashes) and `#` alone → `Paragraph`/text as typed;
  `#\tx` is not a heading; a heading inside a fence is `Code` (T011); indent of 4 spaces → not a
  heading; `**b**` in a paragraph, a heading text `# a **b** c`, a list item, a task item and a table
  cell (`| **b** | c |`) → a `Bold` span with text `b` and no asterisks; `**` unmatched
  (`a ** b`, `**open`, `close**`) → one `Plain` span with the asterisks; `** spaced **` (space after
  the opener) → literal; `***both***` → `BoldItalic`; `\*\*x\*\*` → literal asterisks.
- [X] T020 [US2] Extend `tests/qml/reader_blocks.qml` (T008): heading items' pixel sizes for levels 1–6
  at `fontPx = 38` are `Math.round(38 * [1.9, 1.55, 1.3, 1.15, 1.1, 1.05][level - 1])` (72, 59, 49,
  44, 42, 40) and strictly decreasing for levels 1–3; a heading is bold; after changing the font size
  the same ratios hold; `unsplittable` contains, for every heading, an interval whose `top` is the
  heading's top and whose `bottom` is at least the heading's bottom plus `Math.round(fontPx * 1.4)`
  (keep with next, research R-6); a heading that is the last block of the note still gets a (harmless)
  interval; paging with `bridge.pageTarget` (real implementation via the stubs, or a direct call of
  `pbnotes::page_target` through the bridge) never returns a `contentY` that would leave a heading as
  the last item on a page when the following line fits in the view. Fails until T021/T022.

### Implementation

- [X] T021 [US2] In `qml/BlockText.qml` change `headingPx(level)` to the scale `[1.9, 1.55, 1.3, 1.15,
  1.1, 1.05]` (rounded), keep headings bold. Make the size parts of T020 pass.
- [X] T022 [US2] In `qml/pages/ReaderPage.qml` extend `refreshUnsplittable()`: for every block of kind
  `heading` add `{ top: body.y + it.y, bottom: body.y + it.y + it.height + Math.round(bridge.fontPx *
  1.4) }`; the intervals are in content coordinates like the existing ones. No change to
  `core/src/paging.cpp` (an interval taller than the view is ignored there already). Make T020 pass.
- [X] T023 [MANUAL] [US2] Device check (SC-004): the headings note, A- / A+ and paging to a heading at *(informal: the author tried it on the reader, 2026-10-11, and approved: "works like a charm")*
  a page end. Record in `checklists/device-results.md`; if levels 2 and 3 are hard to tell apart,
  adjust the scale in `BlockText.qml`, `contracts/qml-bridge.md` and T020 together.
- [X] T024 [US2] Run the whole suite. Walk quickstart §3 rows for US2.

**Checkpoint**: User Stories 1 and 2 work independently.

---

## Phase 5: User Story 3 — Insert formatting from a "/" menu in the editor (Priority: P1)

**Goal**: typing "/" at a line start or after a space opens a list of twelve entries next to the
caret; choosing one inserts its Markdown with the caret where typing continues; the insertion is one
undo step; the Title field and "/" after other characters never open it.

**Independent Test**: in the editor type "/" on an empty line → the twelve entries appear above the
keyboard; choose *Code block* → fence, empty line, fence with the caret on the empty line, "/" gone;
Undo → the note as before. (quickstart §4 rows 7–13; US3 scenarios 1–10.)

### Tests first (observe failing)

- [X] T025 [P] [US3] Create `tests/unit/test_slash.cpp` for `pbnotes/slash.h` (header does not exist
  yet, so it fails to compile). `slash_entries()`: 12 entries, ids in the order quoted in
  *Conventions*. `detect_slash(before, after, caret, menu_pos)` (UTF-16, `menu_pos = npos` when no
  menu): `"" → "/"` caret 1 → Menu pos 0; `"a\n" → "a\n/"` → Menu pos 2; `"a " → "a /"` → Menu pos 2;
  `"a\t" → "a\t/"` → Menu; `"a" → "a/"` → None; `"1" → "1/"`, `"x:" → "x:/"`, `"a." → "a./"` → None;
  `"http:/" → "http://"` (caret 7, menu_pos npos) → None; `"" → "//"` (a two-character paste) → None;
  `"a /" → "a //"` caret 4, menu_pos 2 → DateTime pos 3; `"//"` typed when `menu_pos` is npos at a
  boundary (`"/"` → `"//"`, menu_pos npos) → None; `"a b" → "a /b"` (caret 3 = p+1) → Menu pos 2;
  `"ab" → "a/b"` → None; a delete (`"a /" → "a "`) → None; the caret not at p+1 → None; a "/"
  inserted in the middle of existing text after a space (`"a b" → "a / b"`, caret 3) → Menu pos 2; an
  insertion of a non-slash character → None; surrogate pairs before the slash (`"😀 " → "😀 /"`)
  positions counted in UTF-16 units. `expand_slash(entry, text, pos, 1)` returns `SlashEdit{begin,
  end, text, caret}` — for every entry except `Emoji` the exact text and caret of the table in
  `contracts/core-library.md` (e.g. Table text = `"|  |  |\n| --- | --- |\n|  |  |"`, caret `begin+2`;
  Details caret `begin+19`; CodeBlock caret `begin+4`; Bold `****` caret `begin+2`; Link caret
  `begin+1`; InfoBox caret at the end); new-line rule: for a *block* entry (BulletList, Table,
  InfoBox, Details, Quote, CodeBlock) with non-blank text before `pos` on the line (`"abc /"`) the
  replacement gets a leading `\n` and the caret moves by one; with only whitespace or nothing before
  it, no leading `\n`; for multi-line entries (Table, InfoBox, Details, CodeBlock) with text after the
  "/" on the same line (`"/xyz"`) a trailing `\n` is added; BulletList and Quote get none; inline
  entries (Link, Bold, Italic, Underline, Strikethrough) never get any; `expand_slash` for `Emoji`
  returns the plain one-"/" removal (`text` empty). `replace_slash(pos, len, insert)` replaces and
  puts the caret after `insert`. Re-parse check (`parse_markdown` on the result applied to an empty
  note): Table text → one `Table` block with 2 columns, header, delimiter and one body row (3 rows);
  CodeBlock → `Fence`, `Code`, `Fence`; BulletList `"- x"` → `ListItem`. (Info box, quote and details
  re-parse checks are added in T041.)
- [X] T026 [P] [US3] Extend `tests/unit/test_edit_history.cpp` (history use, research R-8; no
  `EditHistory` change expected): `reset("", "ab", "", false)`; `begin_group(Text)`; `observe(Text,
  "ab/")`; `observe(Text, "ab//")`; replace with `observe(Text, "ab2026-10-11 14:30")`; `end_group()`
  → exactly one new undo step (`undo_size() == 1`) with empty `removed` and `inserted ==
  "2026-10-11 14:30"`; `undo()` yields `remove_len == 16` at pos 2 and the text is `"ab"`; `redo()`
  re-applies. A group that is dismissed: `begin_group(Text)`, `observe("a/")`, `end_group()` with
  `current == "a/"` → one step inserting "/" that is not merged into later typing (`observe("a/x")`
  after it is a separate step). A group opened while the grouped field already has earlier typing
  steps does not merge with them. `undo()` while a group is open ends the group first (existing
  behavior; pin it).
- [X] T027 [P] [US3] Extend `tests/bridge/test_edit_bridge.cpp` for `SlashBridge`
  (`contracts/qml-bridge.md`, the class does not exist yet): `entries()` has 12 maps `{id, label}` in
  order, labels non-empty; `detect("", "/", 1, -1)` → `{trigger:"menu", pos:0}`; `detect("a", "a/", 2,
  -1)` → `{trigger:"none"}`; `detect("a /", "a //", 4, 2)` → `{trigger:"datetime", pos:3}`;
  `expand("code_block", "/", 0, 1)` → `{begin:0, end:1, text:"```\n\n```", caret:4}`;
  `expand("emoji", ...)` and an unknown id → `{}`; `replace(3, 1, "🙂")` → text `"🙂"`, caret `3+2`
  (UTF-16 units; a surrogate pair is 2); no call logs or throws.
- [X] T028 [US3] Create `tests/qml/editor_slash.qml` (driver in the style of
  `tests/qml/editor_flow.qml`: the real `qml/pages/EditorPage.qml`, stubs, a real `SlashBridge` and a
  real `EditSession` registered as context properties `slash` and `editSession` in
  `tests/qml/test_qml_pages.cpp`) and register it in `tests/CMakeLists.txt` as `qml_editor_slash`.
  Menu checks: typing "/" into the empty text area opens the menu (`objectName: "slashMenu"` visible)
  with 12 buttons in the specified order; typing "/" after "abc" opens nothing; "https://" and "1/2"
  open nothing; choosing "Code block" replaces the "/" with the fence text, the caret is on the empty
  line (`area.cursorPosition === 4`), the menu is closed, the text area still has active focus;
  `editSession.canUndo` is true and ONE `undo` restores the text exactly as before the "/" was typed;
  `redo` restores the insertion; typing "x" while the menu is open closes it and leaves "/x";
  tapping the scrim closes it and leaves the "/"; `page.backHandled()` returns true and closes it
  when open, false otherwise; a block entry chosen after "abc " inserts a leading line break; a "/"
  typed in the Title field never opens the menu; text set by Paste, Undo, Redo or the attachment
  token never opens it (set `page.programmatic` paths); changing the page width closes the menu;
  opening the long-press menu closes the slash menu and ends its history group. Fails until T034.

### Spike (device) — before T033

- [X] T029 [MANUAL] [US3] Spike S-3 (research R-10): on the reader, with the keyboard shown in the *(informal: the author tried it on the reader, 2026-10-11, and approved: "works like a charm")*
  editor, show a throw-away popup with 6 two-column rows of buttons (a temporary `Rectangle` + `Grid`
  in `EditorPage.qml`, not committed): does it fit above the keyboard, is every button tappable, and
  does Back (hardware or software) reach `onBackRequested` in `qml/Main.qml`? Record in
  `checklists/device-results.md`. If it does not fit, T033 uses the one-column scrolling fallback; if
  Back does not arrive, T033 adds a visible *Close* button to every popup.

### Implementation

- [X] T030 [US3] Create `core/include/pbnotes/slash.h` and `core/src/slash.cpp` per
  `contracts/core-library.md`: `SlashEntry`, `SlashEntryInfo`, `slash_entries()`, `SlashTrigger`,
  `SlashDetected`, `detect_slash`, `SlashEdit`, `expand_slash`, `replace_slash` (leave
  `format_timestamp` and `days_in_month` declared and returning empty/0 until T057; do not implement
  them here). Offsets are UTF-16 units. Add `src/slash.cpp` to `PBNOTES_CORE_SOURCES` in
  `core/CMakeLists.txt`. Make T025 pass.
- [X] T031 [US3] Add `SlashBridge` to `bridge/include/edit_bridge.h` and `bridge/src/edit_bridge.cpp`
  per `contracts/qml-bridge.md` (`entries`, `detect`, `expand`, `replace`; `timestamp`, `daysInMonth`
  and `emojis` come in T058). `QString` ↔ UTF-16 via `QString::utf16()`; labels via `tr()`. The class
  lives in the existing header, so `bridge/CMakeLists.txt` needs no change. Make T027 pass.
- [X] T032 [US3] In `app/main.cpp` create the `SlashBridge` next to `editSession`/`clipboard` and
  expose it as context property `slash`.
- [X] T033 [US3] Create `qml/SlashMenu.qml` per `contracts/qml-bridge.md` (a bordered bubble in the
  look of `SelectionMenu.qml`: white, 4 px black border, radius 14; a 2-column `Grid` of `InkButton`s
  with `focusPolicy: Qt.NoFocus`, `bordered: true`, `font.pixelSize: 30`; `objectName: "slashMenu"`;
  `place()` puts it below the caret line, above it when there is no room above `maxY`, `maxY =
  page.height − keyboardHeight`; a `Flickable` only if taller than the room; no animation) and
  register it in `qml/qml.qrc`. Apply the S-3 fallback if T029 requires it.
- [X] T034 [US3] In `qml/pages/EditorPage.qml` implement the editor flow of `contracts/qml-bridge.md`
  ("Editor flow", steps 1, 2, 4–6): properties `previousText`, `programmatic`, `slashState` (not `slash`, which is the context property); in
  `area.onTextChanged` call `slash.detect(previousText, text, cursorPosition, slashState ? slashState.pos :
  −1)` unless `programmatic`; on `"menu"` call `editSession.beginGroup("text")` **before**
  `editSession.observe("text", text)`, open `SlashMenu` and set `slashState`; any other change while a
  popup is open calls `closeSlash()` (`editSession.endGroup()`, hide, `slash = null`) before
  `observe`; choosing an entry applies `slash.expand(...)` with `programmatic = true` (`area.remove`,
  `area.insert`, `area.cursorPosition`), then `observe` and `endGroup()`; set `programmatic = true`
  around `pasteIntoActive`, `applyHistory`, `insertToken`; `closeSlash()` is also called from
  `onCursorPositionChanged` (when not ours), focus loss, the long-press menu, every header button, the
  category picker, `onWidthChanged`/`onHeightChanged`, and by a transparent full-page scrim
  `MouseArea` (z below the menu, closes on press, swallows the press); the Title field never calls
  `detect`; add `function backHandled()`. Entry `"emoji"` only closes the menu for now (T061 opens
  the grid). Make T028 pass.
- [X] T035 [US3] In `qml/Main.qml` change `onBackRequested` to ask the current page first
  (`var p = nav.currentItem; if (p && p.backHandled && p.backHandled()) return`) before the existing
  pop. Extend `tests/qml/editor_slash.qml` if needed so the Back check goes through `Main`'s logic
  (or call `backHandled()` directly if `Main` is not loadable in the stub).
- [X] T036 [US3] Run the whole suite. Walk quickstart §3 rows for US3.
- [X] T037 [MANUAL] [US3] Device check of quickstart §4 rows 7–13 and 17 on the reader (the list above *(informal: the author tried it on the reader, 2026-10-11, and approved: "works like a charm")*
  the keyboard, the keyboard stays, Back, rotation). Record in `checklists/device-results.md`.

**Checkpoint**: User Stories 1–3 work; the P1 scope is complete.

---

## Phase 6: User Story 4 — Read the other formats the menu inserts (Priority: P2)

**Goal**: quotes, info boxes, strikethrough, underline and collapsible details are shown formatted
on the reading page; the note text is never changed; copying returns the source.

**Independent Test**: a note with a quote, each info kind, `~~struck~~`, `<u>under</u>` and a details
section: each in its form; tap the summary to open and close. (quickstart §4 row 6; US4 scenarios
1–10.)

### Tests first (observe failing)

- [X] T038 [P] [US4] Extend `tests/unit/test_markdown_blocks.cpp`. Inline: `~~x~~` → span `x` with
  `strike`; `<u>x</u>` → `underline`; `**~~both~~**` → one span `Bold` + `strike`; `~~a **b** c~~` →
  three spans all `strike`, middle also `Bold`; `~~a [l](https://x.example) b~~` → the link span keeps
  `url` and `strike`; `<u>` inside a link label; unclosed `~~x`, `x~~`, `~~ x ~~` (space after the
  opener), `~x~`, `~~~x~~~` and unclosed `<u>x` stay literal; table cell `| ~~a~~ | <u>b</u> |`
  → cell spans carry the flags (`cell_spans` keeps them); spans with different flags do not merge;
  source mapping: `src`, `el_begin`, `el_end` cover the marks (`~~` 2 bytes each side, `<u>` 3 and
  `</u>` 4 bytes). Quote: `> a` → `Paragraph` with `quote == Quote`, `quote_first && quote_last`,
  `src_begin == 0`, text `a`; `>a` (no space) works; `> # H` → `Heading` inside a quote; `> - x` →
  `ListItem` inside; two consecutive lines → one group (`quote_first` on the first, `quote_last` on
  the second); a blank line between → two groups; a lone `>` → `Blank` inside the group; a line
  without `>` ends it; 4 spaces of indent → not a quote; a fence opened on a quote line (`> ` followed by three backticks): `Fence`/`Code`
  blocks with `quote == Quote` and `code_first/last`; the fence ends with the quote if unclosed.
  Info box: `> [!NOTE]` → `Callout` (`callout == Note`) with `quote == Info` on it and on the following
  quote lines; `[!tip]`, `[!Warning]  ` (case, trailing spaces) accepted; `[!INFO]` (unknown kind) →
  an ordinary quote whose text is `[!INFO]`; an info header is recognized only as the first line of a
  group (`> a` then `> [!NOTE]` keeps the second as plain quote text). Details: `<details>`, `<summary>S</summary>`,
  `body`, `</details>` → `Marker`(`<details>`), `Summary`(text `S`, `details_id == 0`), `Paragraph`
  (`details_id == 0`), `Marker`(`</details>`); without a summary line the `<details>` line is the
  `Summary` with text `Details`; `<details open>` → `details_open`; case-insensitive tags; unclosed →
  runs to the end; nested `<details>` inside a section → `Paragraph` text as typed; a `<details>` in a
  fence or in a quote → not a section; a `Summary` has `src_end` at the line end; a line `<details> x`
  (text after) → plain paragraph.
- [X] T039 [P] [US4] Extend `tests/unit/test_copy_source.cpp` and
  `tests/unit/test_markdown_source_map.cpp`: a whole selected quote line copies `> text`; part of it
  copies only the characters; a selected `Callout` block copies its line `> [!NOTE]` as written; a
  selection across a collapsed section (the blocks are not special to the core) copies all its lines
  including `<details>`, `<summary>S</summary>` (whole `Summary` selected → full line), body and
  `</details>`; two words inside `~~struck text~~` copy just those words, the whole struck run copies
  with its `~~`; `<u>…</u>` likewise; a `Marker` selected alone copies its line.
- [X] T040 [P] [US4] Extend `tests/bridge/test_reader_blocks.cpp`: kinds `"callout"`, `"summary"`,
  `"marker"`; `quote` 1/2, `quoteFirst`/`quoteLast`, `callout` strings (`"note"`, `"tip"`,
  `"warning"`, `"important"`, `"caution"`), `detailsId`, `detailsOpen`; `html` of `~~x~~` is
  `<s>x</s>`, of `<u>x</u>` is `<u>x</u>`, of `**~~x~~**` is `<b><s>x</s></b>`; note text such as
  `<script>` or `<details>` inside a paragraph is escaped (`&lt;script&gt;`); table cells carry
  `<s>`/`<u>` html; blocks outside any new construct keep their old keys and values (`quote == 0`,
  `detailsId == -1`).
- [X] T041 [P] [US4] Extend `tests/unit/test_slash.cpp` with the remaining re-parse checks: InfoBox
  text → `Callout` (`Note`) + one quote `Paragraph` with `quote == Info`; Quote `"> x"` → `Paragraph`
  with `quote == Quote`; Details text → `Marker`, `Summary` (text empty), `Blank`, `Marker`; Underline
  and Strikethrough texts (`<u></u>`, `~~~~`) with typed content parse to flagged spans.
- [X] T042 [US4] Extend `tests/qml/reader_blocks.qml`: a closed section shows its `summary` row with
  `▶` and its content items `visible === false`; a tap on the summary shows `▼` and the content; a
  second tap hides it; `<details open>` starts open; the reader's `openDetails` state survives
  `page.load()`; a tap on a summary while a selection exists only ends the selection; hidden units
  are skipped by the marker hit-testing (`unitAtY`); a `quote == 1` block shows a 6 px left bar
  (`objectName: "quoteBar"`), `quote == 2` draws the box border on its group edges
  (`objectName: "infoBox"`), the `callout` row shows the kind text ("Note", "Tip", "Warning",
  "Important", "Caution") in bold; a `marker` has height 0 and is not drawn; `unitIsText` is false for
  `callout` and `marker`. Fails until T049/T050.

### Spike (device) — before T049

- [X] T043 [MANUAL] [US4] Spike S-2 (research R-5): a paragraph and a list item with `<s>` and `<u>` *(informal: the author tried it on the reader, 2026-10-11, and approved: "works like a charm")*
  text at the three reading sizes (temporary note, no code change): is the line through/under visible
  and does it leave the glyphs readable? Record in `checklists/device-results.md`.

### Implementation

- [X] T044 [US4] Inline flags in `core/src/markdown_inline.cpp`: extend `add_span` (and
  `markdown_internal.h`) with `bool strike = false, bool underline = false` — spans merge only when
  kind, link, `link_confirm`, `strike` and `underline` are all equal; `add_link` and `cell_spans`
  pass the flags through; in `parse_inline` add the `~~` branch (run of exactly two `~`, opener
  followed by a non-space, closer found like the `**` closer, inner parsed with `depth + 1`, the flag
  set on every inner span, the outer kind kept; source extents `eb`/`ee` recorded as for `**`) and the
  `<u>`…`</u>` branch (lower-case tags, same rules, closer `</u>`). Unmatched marks stay literal. The
  "outer emphasis wins" collapsing of inner kinds for emphasis containing links is unchanged. Make
  the inline parts of T038 pass; T002 stays green.
- [X] T045 [US4] Quote and info box in `core/src/markdown.cpp`: factor the line-handling part of
  `parse_markdown` into a helper that can parse "the rest of a line" at an offset (so a quote line's
  remainder runs through the same heading/list/fence/paragraph code with `body_at` offset by the
  marker, keeping `src_begin = 0`); detect a quote line per *Conventions* (not for a table candidate:
  `read_table` is skipped for `>` lines already); set `Block::quote`; the info header line becomes a
  `Callout` block (`callout` from the kind, `src_begin = 0`, `src_end = line.size()`, no spans) and
  makes the group `Info`; a post-pass sets `quote_first`/`quote_last` per group (a group ends at a
  non-quote block, i.e. at a blank line or any line without `>`; a lone `>` line is a `Blank` inside the group). A fence opened inside a quote ends at its closer or when the quote ends. Update
  `ends_table` in `core/src/markdown_table.cpp` if needed (a quote line already ends a table). Make
  the quote/info parts of T038 pass.
- [X] T046 [US4] Details in `core/src/markdown.cpp` per *Conventions*: recognize `<details>` /
  `<details open>` / `</details>` / `<summary>…</summary>` lines at top level only; emit `Marker`,
  `Summary` (inline spans of the summary text, `src_begin = 0`, `src_end = line.size()`) and tag
  content blocks with `details_id` (the 0-based line of the `<details>` line) and `details_open`; the
  `<details>` line itself becomes the `Summary` (text "Details") when no summary line follows; nested
  `<details>` lines inside a section are ordinary text; an unclosed section ends at the end of the
  note. Make the details parts of T038 pass.
- [X] T047 [US4] In `core/src/markdown_copy.cpp`: `Callout` and `Marker` copy their whole line
  (like `Fence`), `Summary` is a text unit (`is_text()`), and a quote block's `src_begin == 0` already
  yields the `> ` prefix on a whole selection. Make T039 pass.
- [X] T048 [US4] In `bridge/src/reader_blocks.cpp`: `styled()` wraps in `<s>` / `<u>` from the span
  flags (order `<b><i><s><u>…`, closing in reverse); emit `quote`, `quoteFirst`, `quoteLast`,
  `callout`, `detailsId`, `detailsOpen` on every text-like entry (defaults `0`, `false`, `""`, `-1`);
  the `summary` entry's `html` is its spans; `marker` and `callout` entries have empty `html`
  (the callout's label is drawn by QML from `callout`); table-cell html uses the same `styled()`.
  Make T040 pass.
- [X] T049 [US4] In `qml/BlockText.qml`: a `quote == 1` block draws a 6 px black bar at its left and
  indents its text by 24 px; `quote == 2` draws a 3 px black border box around its group (top edge on
  `quoteFirst`, bottom on `quoteLast`, sides on every block); a `callout` row shows
  `qsTr("Note")`/`Tip`/`Warning`/`Important`/`Caution` in bold at body size; a `summary` row shows
  `▶ ` (closed) or `▼ ` (open) before its text, the whole row is a `MouseArea` that emits
  `summaryToggled(id)`; a `marker` has `implicitHeight` 0 and `visible: false`. Quote inside code (a
  fence in a quote) draws both frames: bar/border outside, code box inside. Make the drawing parts of
  T042 pass.
- [X] T050 [US4] In `qml/pages/ReaderPage.qml`: `property var openDetails: ({})`; `isOpen(b)` =
  `openDetails[b.detailsId]` if defined else `b.detailsOpen`; content blocks of a closed section get
  `visible: false` (wrap the delegate's `visible`), the `summary` toggles its entry (assign a new
  object so bindings update) unless `selectionTapGuard()`; `unitIsText` false for `callout` and
  `marker`; `unitAtY` and `pointOf` skip units that are not `visible`; `refreshUnsplittable` skips
  invisible blocks. Make T042 pass.
- [X] T051 [US4] Run the whole suite (T002's golden test included). Walk quickstart §3 rows for US4.
- [X] T052 [MANUAL] [US4] Device check of quickstart §4 row 6: quote bar, info boxes labelled in black *(informal: the author tried it on the reader, 2026-10-11, and approved: "works like a charm")*
  and white, strike and underline, details open/close with a finger, unclosed section. Record in
  `checklists/device-results.md`.

**Checkpoint**: User Stories 1–4 work; every format the menu inserts is also displayed.

---

## Phase 7: User Story 5 — Insert a date and time, and emojis, from pickers (Priority: P2)

**Goal**: "//" opens a date-time picker (starts at now) whose Insert writes `YYYY-MM-DD HH:MM`; the
Emoji entry opens a grid whose tap inserts one emoji; both are one undo step and cancel without
loss.

**Independent Test**: in the editor type "//" on an empty line → the picker shows now; change day and
hour; Insert → the stamp appears and the "//" is gone; Undo removes it. Type "/", choose *Emoji*, tap
one → inserted. (quickstart §4 rows 14–15; US5 scenarios 1–10.)

### Tests first (observe failing)

- [X] T053 [P] [US5] Extend `tests/unit/test_slash.cpp`: `format_timestamp(2026, 10, 11, 14, 30) ==
  u"2026-10-11 14:30"`; `(2026, 1, 2, 3, 4) == u"2026-01-02 03:04"`; `(1970, 1, 1, 0, 0)`;
  `(2099, 12, 31, 23, 59)`; out-of-range arguments are clamped (`(2026, 13, 32, 24, 60)` →
  `u"2026-12-31 23:59"`, `(1900, 0, 0, -1, -1)` → `u"1970-01-01 00:00"`); a day above the month's
  length is clamped to it (`(2026, 2, 31, 0, 0)` → `u"2026-02-28 00:00"`); `days_in_month`: 1→31,
  2→28, 3→31, 4→30, 5→31, 6→30, 7→31, 8→31, 9→30, 10→31, 11→30, 12→31; `(2024, 2) == 29`, `(2100, 2)
  == 28`, `(2000, 2) == 29`, `(1900, 2)` clamps year to 1970 → 28; month 0 or 13 clamps.
- [X] T054 [P] [US5] Extend `tests/bridge/test_edit_bridge.cpp`: `slash.timestamp(2026, 10, 11, 14,
  30) == "2026-10-11 14:30"`; `slash.daysInMonth(2024, 2) == 29`; `slash.emojis()` has exactly 48
  entries, each a single code point (`QString::toUcs4().size() == 1`), all distinct, none a variation
  selector or joiner; every emoji has a glyph in the bundled emoji font: load
  `PBNOTES_EMOJI_FONT` (as `app/main.cpp` does on the host; the test gets the path from the same
  compile definition or the `PBNOTES_EMOJI_FONT` environment variable and SKIPs with exit code 77 when
  the font cannot be found) with `QRawFont` and assert `supportsCharacter(c)` for each.
- [X] T055 [US5] Extend `tests/qml/editor_slash.qml`: typing "/" then a second "/" directly after
  closes the menu and opens the picker (`objectName: "dateTimePicker"` visible, menu hidden), preset
  to the stub clock's now (give the page a `property var now` seam used only by tests, default `new
  Date()`); pressing the day `▲` of a 31-day month at day 31 wraps to 1 (valid days only); changing
  the month from March 31 to February clamps the day to 28 (29 in 2024); Insert replaces the "//" with
  `YYYY-MM-DD HH:MM` of the shown values, closes the picker, caret after the stamp, ONE `undo`
  restores the text from before the first "/" and `redo` re-inserts; Cancel and `backHandled()` close
  the picker and leave "//" as typed (and the history shows an ordinary step for it); "//" typed after
  a letter (`a//`), as in `https://`, opens nothing; "//" in the Title field opens nothing; choosing
  *Emoji* opens `objectName: "emojiPicker"` with 48 cells; tapping a cell replaces the "/" with that
  emoji, closes the grid, one `undo` restores; Cancel / Back leave the "/"; rotation (width change)
  closes the picker and leaves the text. Fails until T061.

### Spike (device) — before T059

- [X] T056 [MANUAL] [US5] Spike S-4 (research R-11): on the reader show the 48 emojis of the list in a *(informal: the author tried it on the reader, 2026-10-11, and approved: "works like a charm")*
  throw-away grid and a mock of five `▲ value ▼` columns of `InkButton`s (temporary, not committed):
  do all emojis show as glyphs (no boxes) and are the cells and steppers hit reliably with a finger?
  Record in `checklists/device-results.md`; drop failing emojis from the list in T058 (keep 48 by
  substituting from the same font), enlarge cells if needed.

### Implementation

- [X] T057 [US5] Implement `format_timestamp` and `days_in_month` in `core/src/slash.cpp` per
  `contracts/core-library.md` (Gregorian leap rule: divisible by 4 and not by 100, or by 400; clamp
  ranges quoted in *Conventions*). Make T053 pass.
- [X] T058 [US5] Add `timestamp(...)`, `daysInMonth(...)` and `emojis()` to `SlashBridge`
  (`bridge/src/edit_bridge.cpp`): `emojis()` returns the fixed list of 48 single-code-point emojis
  (common faces, hands, hearts, check/cross/warning/question, pin, calendar, clock, note, book,
  camera, party, gift, coffee, house, car, plane, sun, rain, star, fire, light bulb, paperclip —
  choose only code points the bundled Noto Emoji font covers; T054 proves it). Make T054 pass.
- [X] T059 [US5] Create `qml/DateTimePicker.qml` per `contracts/qml-bridge.md` (a centred bubble in
  the style of `SlashMenu`, `objectName: "dateTimePicker"`; five columns year, month, day, hour,
  minute, each `▲` / value / `▼` as `InkButton`s of at least 78 px height with `focusPolicy:
  Qt.NoFocus`; range and clamping as in *Conventions* using `slash.daysInMonth`; `Insert` emits
  `accepted(slash.timestamp(...))`, `Cancel` emits `cancelled()`; no animation) and register it in
  `qml/qml.qrc`.
- [X] T060 [US5] Create `qml/EmojiPicker.qml` (a centred bubble, `objectName: "emojiPicker"`, a 6×8
  grid of buttons each at least 78 × 78 px showing one emoji from `slash.emojis()`, `chosen(emoji)`,
  `Cancel` → `cancelled()`, `focusPolicy: Qt.NoFocus`) and register it in `qml/qml.qrc`.
- [X] T061 [US5] In `qml/pages/EditorPage.qml` finish the flow ("Editor flow" steps 1 `datetime`, 2
  emoji, 3): on `"datetime"` close the menu, show `DateTimePicker` preset from `page.now`, keep the
  history group open; `accepted(text)` applies `slash.replace(slashState.pos, 2, text)` with `programmatic =
  true`, then `observe` and `endGroup()`; the menu entry `"emoji"` opens `EmojiPicker` (the group
  stays open), `chosen(e)` applies `slash.replace(slashState.pos, 1, e)` the same way; `cancelled()` and
  `backHandled()` call `closeSlash()`; both pickers close on rotation and on any scrim tap; the text
  area keeps focus under them. Make T055 pass; T028 stays green.
- [X] T062 [US5] Run the whole suite. Walk quickstart §3 rows for US5.
- [X] T063 [MANUAL] [US5] Device check of quickstart §4 rows 14–15 and 17. Record in *(informal: the author tried it on the reader, 2026-10-11, and approved: "works like a charm")*
  `checklists/device-results.md`.

**Checkpoint**: all five user stories work.

---

## Phase 8: Polish & cross-cutting concerns

- [X] T064 [P] Add to `tests/integration/test_edge_cases.cpp` a note that holds every new format
  (a `yaml` fence, a `~~~` fence, quote, each info kind, `~~`, `<u>`, a details section, `#`..`######`,
  bold, plus text that merely looks like them: `#tag`, an unclosed fence, `<details>` in a fence): save
  it through the store, push it with the fake server, pull it back; the text is identical byte for byte
  at each step (FR-019, SC-006), `find_attachment_refs` finds the same references as before, and
  `parse_markdown` of it does not crash or hang for lines up to 20,000 bytes.
- [X] T065 Review: run `scripts/check-constitution.sh`; grep the diff for `qDebug`, `qWarning`,
  `log::` calls and confirm none carries typed, inserted or note text (event names and counts only);
  confirm the bridge never inserts note text unescaped into `html` (T040 covers it); confirm no new
  dependency, no setting and no stored field was added; confirm every new QML file is in `qml.qrc`
  and no Q_OBJECT header is missing from `pbnotes_moc`.
- [X] T066 [P] Update `README.md` (feature list: code blocks, quote/info box/strikethrough/underline/
  details, the "/" menu and the "//" date-time picker; the Markdown forms the reader understands and
  the stored text each insertion writes) and `INSTALL.md` only if T017 added a font file.
- [X] T067 Release: raise `VERSION` from `1.2.0` to `1.3.0` (MINOR: a user-visible feature, no breaking
  change), add the `## 1.3.0` entry to `CHANGELOG.md` (code blocks; clearer headings; quote, info box,
  strikethrough, underline, collapsible details; the "/" menu with twelve entries; the "//" date-time
  picker; the emoji grid; nothing stored or changed in sync), rebuild, and run
  `scripts/check-version-bump.sh` as `scripts/pre-merge.sh` does. Tag `v1.3.0` after the merge.
- [X] T068 Run `scripts/pre-merge.sh` (all tests, constitution checks, version bump) on the final tree
  and fix what it reports.
- [X] T069 [MANUAL] Full quickstart §4 on the reader with the release package (all 17 rows), including *(informal: the author tried it on the reader, 2026-10-11, and approved: "works like a charm")*
  landscape. Record in `specs/006-markdown-slash-menu/checklists/device-results.md` and update
  `research.md` R-6/R-7/R-10/R-11 with the spike results and any changed numbers (heading scale, grid
  size).
- [X] T070 Update `specs/006-markdown-slash-menu/checklists/requirements.md` notes: every FR-001..
  FR-021 is covered by a task above (map: FR-001..FR-004 → T004–T018; FR-005, FR-006 → T019–T024;
  FR-007..FR-011 → T025–T037; FR-012..FR-015 → T053–T063; FR-016..FR-018 → T038–T052; FR-019 →
  T044, T064; FR-020 → T002, T005, T064; FR-021 → T065); list the open [MANUAL] tasks.

---

## Dependencies & Execution Order

- Phase 1 (T001, then T002) first. Phase 2 (T003) needs T001; T002 stays green through it.
- **US1** (Phase 3) needs T003. Tests T004 ‖ T005 ‖ T006 ‖ T007 are independent files; T008 is its
  own file (QML). T010 → T011 (needs T004) → T012 (needs T005) → T013 (needs T006) → T014 (needs
  T007) → T015/T016 (need T008; T015 needs T009's result for the font, T017 only if T009 fails).
- **US2** (Phase 4) is independent of US1 in the parser (T019 is a new file) but T020 extends the QML
  test file created in T008 and T021/T022 edit the files US1 edited (`BlockText.qml`,
  `ReaderPage.qml`): do US2 after US1's T016.
- **US3** (Phase 5) needs T003 and nothing from US1/US2 except that T025's re-parse of CodeBlock needs
  T011. Tests T025 ‖ T026 ‖ T027; T028 needs T025–T027's targets only for compiling the stubs. T030 →
  T031 → T032; T033 needs T029's result; T034 needs T030–T033 and edits `EditorPage.qml` (sequential
  with T061); T035 after T034.
- **US4** (Phase 6) needs US1 (the shared parse path and `BlockText` code box) and, for T041, US3's
  `test_slash.cpp`. Tests T038 ‖ T039 ‖ T040 ‖ T041; T042 extends the QML file from T008. T044 → T045
  → T046 (same files, sequential); T047 needs T045/T046; T048 needs T044–T046; T049 needs T042/T043;
  T050 after T049 (same page).
- **US5** (Phase 7) needs US3 (the menu, the editor flow, `SlashBridge`). Tests T053 ‖ T054; T055
  extends `editor_slash.qml` from T028. T057 → T058 → T059 ‖ T060 → T061.
- Polish needs the stories it documents or runs. T067 (version bump) comes last before the merge.

### Parallel opportunities

- Phase 3 tests: T004 ‖ T005 ‖ T006 ‖ T007. Phase 5 tests: T025 ‖ T026 ‖ T027. Phase 6 tests: T038 ‖
  T039 ‖ T040 ‖ T041. Phase 7 tests: T053 ‖ T054. Phase 7: T059 ‖ T060.
- US3 (core + bridge + editor) can proceed in parallel with US1/US2 (reader) by two people: they touch
  different files until T061 (editor) and T049/T050 (reader).
- Polish: T064 ‖ T066.

## Implementation Strategy

- **MVP = Phase 1 + Phase 2 + US1** (T001–T018): code blocks read correctly, with the shared fence
  rule protecting attachments and tasks. Ship-ready on its own.
- Then US2 (small, display only), US3 (the editor menu — the P1 scope ends here), and last US4 and
  US5 (P2). US4 is the largest (parser growth, three drawing paths, details state); US5 is small once
  US3 exists.
- Every code task is preceded by its failing test; the [MANUAL] tasks and the four spikes are the only
  device steps and are reported as open by an implementing agent. T017 (bundled font) is conditional
  on spike S-1.
