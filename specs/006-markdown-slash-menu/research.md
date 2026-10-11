# Research: Better Markdown Support and Slash Menu

Resolves what the plan's Technical Context left open. Facts were read from this repository (core,
bridge, QML, tests, features 002–005 specs and plans); nothing was probed on the device. Items
marked **[SPIKE]** need a short check on the InkPad before the work that depends on them, and have a
stated fallback.

## R-1. One fence rule for the whole code base (FR-001 – FR-003)

- **Findings**: `parse_markdown`, `find_attachment_refs` and `tokenize_attachment_refs` each toggle a
  boolean on any line that starts (after any indentation) with three backticks; `read_table`'s
  `ends_table` tests the same prefix. Inside a fence the parser already emits each line as a literal
  span (no emphasis, no links), but the fence lines themselves are shown as text and the layout is
  that of ordinary paragraphs.
- **Decision**: one helper in `markdown_internal.h`: `open_fence(line)` → `{ch, count}` or none
  (indent ≤ 3 spaces — a tab counts 4 —, three or more `` ` `` or `~`; for a backtick fence the info
  string after it must not contain a backtick), and `closes_fence(line, open)` (same character, at
  least as many, only spaces after). All four sites call it, so attachments, task toggles and tables
  ignore exactly what the reader shows as code (FR-020 – attachments/tables behave as before outside
  code). An unclosed fence runs to the end of the note (FR-003). The info string (`yaml`) is kept as
  `Block::fence_info` only for tests; it is never displayed (scenario 7).
- **Alternatives considered**: a regex in each place (drifts apart again); a full CommonMark block
  parser (far beyond five formats, constitution II).

## R-2. Containers as flags on per-line blocks (FR-001, FR-016, FR-018; scenarios 1.5, 1.8)

- **Findings**: the reader's selection units, `copy_source` (`SourcePoint{block,row,byte}`), the
  source map (`src_begin/src_end`, `Span::src`) and the page's `unsplittable` list all treat *one
  block = one source line (or one table row)*. Feature 005's scenarios (partial selection inside a
  bold phrase, whole-element marks) depend on it.
- **Decision**: keep that. Four new kinds: `Fence` (an opening or closing fence line), `Code` (a
  line inside a fence), `Callout` (the `> [!NOTE]` header line of an info box) and `Summary` (the
  summary line of a details section), plus `Marker` (structural `<details>` / `</details>` lines that
  are never shown). `Block` gets `quote` (none / quote / info), `details_id` (−1 or the 0-based line
  of the section's `<details>` line, a stable key) and, from a post-pass, `code_first/last` and
  `quote_first/last`: the first and last block of each box, so the QML can draw top, bottom and side
  borders per block and the result reads as one frame. `Fence` and `Callout` blocks are selected
  whole (like a blank line or an attachment) and copy their line as written; `Code` blocks are text
  units with the literal spans, so a character range inside code copies exactly those characters.
- **Why not one block per box**: `copy_source` for a range inside a 30-line code block would need a
  second coordinate system (line, column) and a second selection model in `ReaderPage.qml`; hidden
  `Marker`s and per-line blocks need none of that. The cost (500 code lines = 500 delegates) equals
  today's, because every fenced line is already one block (SC-008 holds by construction).
- **Alternatives considered**: a nested block tree (what CommonMark does) — correct but needs a new
  addressing scheme for selection, copy and paging; per-box blocks with an inner line list (see
  above).

## R-3. Quote and info box (FR-016; scenarios 4.1, 4.2, 4.7)

- **Findings**: `parse_markdown` already skips table detection when a line starts with `>`
  (`rest[0] != '>'`), i.e. a quote was anticipated but never parsed.
- **Decision**: a line with ≤ 3 spaces of indent, `>` and an optional single space is a quote line;
  the marker (and that space) is stripped and the rest is parsed as an ordinary line, so a heading,
  list item, bold text or a fence can sit inside a quote. The block records `src_begin = 0`, so a
  completely selected quote line copies with its `> `, like `# ` for a heading. `Block::quote` is
  `Quote`; if the first line of a run of quote lines is, ignoring case and trailing spaces,
  `[!NOTE]`, `[!TIP]`, `[!WARNING]`, `[!IMPORTANT]` or `[!CAUTION]`, that line becomes a `Callout`
  block (its kind in `Block::callout`) and the whole run is `Info`. Consecutive quote lines form one
  group; a blank line, or any line without `>`, ends it. The `[!KIND]` header is recognized only as
  the first line of a group; later in a group it is ordinary text. A lone `>` line is a blank line inside the group. Lazy continuation (a line without
  `>`) is not supported — it ends the quote. Nested quotes (`> >`) are shown as a quote whose text
  starts with `>` (out of scope, Assumptions).
- **Display**: a plain quote has a 6 px bar at the left and the text indented; an info box has a
  3 px border box and its header shows the kind as text ("Note", "Tip", "Warning", "Important",
  "Caution") in bold — never colour (FR-016).
- **Alternatives considered**: `:::info` fences (not what other Markdown apps write); icons without
  labels (not readable in black and white).

## R-4. Details sections (FR-018; scenarios 4.5, 4.6, 4.10)

- **Decision**: at top level (not in a fence, not in a quote, not already in a section) a line that is
  only `<details>` or `<details open>` (case-insensitive, ≤ 3 spaces indent, trailing spaces ok)
  starts a section; a line that is only `</details>` ends it; an unclosed section runs to the end of
  the note. If the next line is `<summary>…</summary>` (alone on its line) it is the `Summary` block
  (its text is parsed inline), the `<details>` line is a hidden `Marker`; otherwise the `<details>`
  line itself becomes the `Summary` block with the text "Details". Everything between belongs to the
  section (`details_id`); a `<details>` line inside a section is ordinary text (the nested case of
  the spec's edge cases).
- **Open/closed state** is display state in the reading page, keyed by `details_id` (a plain
  `{id: true}` object), initial value from `<details open>`. It survives the page reloads that
  attachment downloads cause (the page keeps the object), is lost when the page closes, and is never
  stored. Hidden content blocks are `visible: false` (no layout space; selection hit-testing skips
  invisible units). A selection spanning a collapsed section still *copies* its text (copy works
  from the source, not from what is visible).
- **Display**: the summary line shows `▶` (closed) / `▼` (open) and its text; the whole line is the
  tap target; no animation.
- **Alternatives considered**: a toggle that edits the note (`<details open>`) — would change the
  note on a read-only page (constitution VI, FR-019).

## R-5. Strikethrough and underline as span flags (FR-017; scenarios 4.3, 4.4)

- **Findings**: `Span::kind` is one of Plain/Bold/Italic/BoldItalic; `add_span` merges neighbours with
  equal kind and link; emphasis with links inside keeps the links but collapses inner kinds
  ("the outer emphasis wins"); `cell_spans` rebuilds spans through `add_span(kind, text, url,
  confirm)` and would drop any new field.
- **Decision**: add `bool strike`, `bool underline` to `Span`. `~~text~~` (exactly two tildes, the
  opener followed by a non-space, a closer preceded by a non-space, found like the `**` closer) and
  `<u>text</u>` (lower-case tags) parse the inside recursively and set the flag on every inner span,
  so `**~~both~~**` and `~~a [link](https://x.example) b~~` keep everything. `add_span` merges only
  spans with equal flags; `add_link` and `cell_spans` pass the flags through. Unclosed or malformed
  marks stay literal text (scenarios 4.3/4.4). Source offsets (`src`, `el_begin/el_end`) are
  recorded as for `**`, so partial/complete selection copies correctly.
- **Bridge**: `styled()` emits `<s>…</s>` and `<u>…</u>`; a link already wraps its text in `<u>`, so
  underlined link text looks like any link (still followable, spec edge case).
- **[SPIKE S-2]**: on the InkPad, `<s>` and `<u>` render a visible line at all three reading sizes
  (the device's Qt rich-text engine is the same as the host's, so this is a legibility check, not a
  support check). No design fallback is expected.
- **Alternatives considered**: widening `Kind` to a bit set (touches every `switch`); extra
  `Span::Kind` values (Strike, Underline, …× Bold/Italic is a combinatorial explosion).

## R-6. Headings and bold you can tell apart (FR-005, FR-006; US2)

- **Findings**: parsing already handles `#`..`######` (space required) and `**`/`__`/`*`/`_`. The
  display uses scales 1.6 / 1.4 / 1.25 / 1.15 / 1.1 / 1.05 and bold for every heading; at the
  medium reading size level 2 and level 3 differ by about 5 px. Bold runs rely on the face having a
  bold weight or on synthesized bold.
- **Decision**: parsing unchanged (only the shared fence rule affects it: a `#` line inside code is
  literal, already true). Display scales become 1.9 / 1.55 / 1.3 / 1.15 / 1.1 / 1.05 (about 72 / 59
  / 49 / 44 / 42 / 40 px at 38 px body), still bold, plus the existing 10 px gap; level 4–6 differ
  from a bold phrase in a paragraph by size and by standing alone on a line. Checked on the device
  against SC-004; the numbers are the only thing that may change after the walkthrough.
- **Keep with next**: the reading page marks each heading's rectangle plus one body line below it as
  an unsplittable interval for the existing `page_target`; a page break can then never fall between
  a heading and its first line, so a heading is not the last item of a page (FR-005, scenario 2.7).
  An interval taller than the view is ignored by the core already, so this can never stall paging.
  No core change.
- Whether bold is visibly heavier on the device is checked in **[SPIKE S-1]** (R-7).

## R-7. Fixed-width type for code (FR-001) — **[SPIKE S-1]**

- **Findings**: no QML sets a font family; the application font is the device default with the emoji
  font behind it (`app/main.cpp`). The reader's text blocks already put the whole text in a
  `white-space: pre-wrap` span, so spaces and indentation are preserved byte for byte
  (`SelectableText.qml`).
- **Decision**: code lines use `font.family: "monospace"` with `styleHint: Font.Monospace` and
  `fixedPitch: true`, at the reading font size × 0.9 (so 80 columns fit the portrait width
  at the medium size).
- **[SPIKE S-1]**: on the InkPad, show a note with a fenced block of `iiii` / `MMMM` lines and
  bold text; pass = the glyph widths are equal (fixed pitch), indentation lines up, and bold text is
  visibly heavier than regular. **Fallback**: bundle one small OFL monospace font next to the emoji
  font (`pbnotes-mono.ttf`, added with `QFontDatabase::addApplicationFont`, license in
  `NOTICE`/`licenses/`) — complexity item C-3. Missing font file is not an error (falls back to the
  default face).
- **Alternatives considered**: a proportional face with pre-wrap (indentation kept but columns do
  not align, the point of a code block); drawing code with own glyph metrics (absurd).

## R-8. Detecting a typed "/" and keeping it one undo step (FR-007 – FR-012)

- **Findings**: the on-screen keyboard is the reader's native one, so key events are unreliable for
  composed input; feature 005 therefore watches *values* (`EditHistory::observe` diffs the new text
  against the known one). Programmatic changes (paste, undo, token insert) also raise `textChanged`.
- **Decision**: the editor keeps the previous text of the area; in `onTextChanged`, unless the
  change is its own (`programmatic` flag set around paste, undo/redo, attachment token, menu
  insertion), it asks `slash.detect(before, after, caret, menuSlashPos)`:
  - **Menu** — `after` equals `before` with one `/` inserted at `p`, the caret is at `p+1`, and
    the character before `p` is nothing (start of text), `\n`, space or tab.
  - **DateTime** — the same, with `menuSlashPos == p-1` (the menu is open on the `/` just before).
  - otherwise none (`a/b`, `1/2`, `https://`, a pasted run, a delete).
  Titles are a different field and are never asked (FR-010).
- **Undo**: on **Menu**, the editor calls `editSession.beginGroup("text")` and then `observe(...)`,
  so the typed `/` is *inside* a group whose start value is the text without it. The second `/`
  (**DateTime**) is also inside it. Choosing an entry, the emoji or the date replaces the text and
  ends the group: the history sees one step "nothing → inserted text" (FR-011). Dismissing the popup
  (any other character, a tap, caret move, focus change, rotation, Back, another command) calls
  `endGroup()` *before* the next observation, so the `/` becomes an ordinary step and nothing is lost
  (FR-009). No change to `EditHistory` is needed; tests pin this use of `begin_group` across several
  observations.
- **Alternatives considered**: a new `EditHistory::amend_last()` (more API, needed only because the
  group is opened too late); filtering menu entries as the reader types (YAGNI, Assumptions).
- **Edge**: a one-character paste cannot be told from typing by value alone; paste goes through the
  editor's own command, which sets `programmatic`, so it never triggers (spec edge case).

## R-9. What each entry inserts (User Story 3, table)

- **Decision**: `expand_slash(entry, text, slash_pos, slash_len)` returns the range to replace
  (`slash_pos`, `slash_len` = 1) and the text and caret (`‸` marks the caret, `⏎` a line break):

  | Entry | Replacement | Starts a new line |
  |-------|-------------|-------------------|
  | Bullet list | `- ‸` | yes |
  | Table | a two-column header row with empty cells (caret in the first), the separator row `--- / ---`, one empty body row; exact text in the contract | yes |
  | Info box | `> [!NOTE]` ⏎ `> ‸` | yes |
  | Link | `[‸](https://)` | no |
  | Bold | `**‸**` | no |
  | Italic | `*‸*` | no |
  | Underline | `<u>‸</u>` | no |
  | Strikethrough | `~~‸~~` | no |
  | Details | `<details>` ⏎ `<summary>‸</summary>` ⏎ (empty line) ⏎ `</details>` | yes |
  | Quote | `> ‸` | yes |
  | Code block | three backticks ⏎ `‸` ⏎ three backticks | yes |
  | Emoji | (picker, then the emoji alone) | no |

  "Starts a new line" = if the line holds text before the `/` (anything but start or whitespace
  only), a `\n` is added in front; if the replacement is several lines and text follows the `/` on
  the line, a `\n` is added after, so the markup stays valid (scenario 3.10). Contract gives the
  exact strings; `test_slash` parses every result with the reader's own parser (the table parses
  as a Table, the fence as `Fence`/`Code`/`Fence`, the info box as `Callout`, the details as
  `Summary`) so what is inserted is what is shown.
- **Alternatives considered**: inserting with example text ("Column 1", "Note…") — the reader would
  have to delete it before typing; empty cells/lines with the caret placed is faster.

## R-10. The popups on e-ink (FR-009, FR-010) — **[SPIKE S-3]**

- **Findings**: `SelectionMenu` is a bubble with a `Flow` of buttons that never take focus
  (`focusPolicy: NoFocus`) so the keyboard stays; the editor passes `maxY = page.height −
  keyboardHeight` to keep it above the keyboard. Back/Escape reaches the app as
  `device.backRequested`, which `Main.qml` answers by popping the page.
- **Decision**: `SlashMenu.qml` is a bubble of the same look with a 2-column grid of buttons in the
  required order (row by row: bullet list, table / info box, link / bold, italic / underline,
  strikethrough / details, quote / code block, emoji), placed under the caret line, above it when
  there is no room above the keyboard, inside a `Flickable` only if still too tall (scenario 3.9). A
  transparent full-page `MouseArea` behind it closes it on any outside tap (the first tap only
  closes, FR-009). `Main.qml` asks the current page first: `if (page.backHandled && page.backHandled())
  return`; the editor answers true when a popup was open and closed it.
  The date-time picker and the emoji grid use the same bubble style but are centred and larger; the
  text area keeps the focus under them (no TextField inside), so the keyboard does not flicker.
- **[SPIKE S-3]**: on the InkPad, with the keyboard shown, (a) the 2-column list fits above it and
  every button is tappable, (b) the hardware/software Back reaches `onBackRequested`. **Fallback**:
  (a) a one-column scrolling list with page-sized steps; (b) a visible *Close* entry at the end of
  each popup.
- **Alternatives considered**: `QtQuick.Controls.Popup`/`Menu` (focus and overlay behaviour on e-ink
  unknown, and they take focus); the editor's header (no room, FR-007 wants it at the caret).

## R-11. Date-time picker and emoji grid (FR-012 – FR-015)

- **Decision (date-time)**: five columns (year, month, day, hour, minute), each a tall `▲` / value /
  `▼` stack of `InkButton`s (≥ 78 px, the app's tap size), preset from the current local time.
  `days_in_month(year, month)` (core, with leap years) sets the day column's range; changing month
  or year clamps the day (31 Mar → Feb gives 28/29) so a non-existent date cannot be chosen. The minute moves
  in steps of five from the value shown (snapping to a multiple of five), because one-minute steps would need up
  to 59 taps; the exact minute of "now" is what Insert writes when nothing is touched, and the picker shows the
  text it will insert. The keyboard is hidden while a picker is open (the buttons need the room) and comes back when
  it closes.
  *Insert* writes `format_timestamp(...)` = `YYYY-MM-DD HH:MM` (24 h, fixed — spec Clarifications);
  *Cancel* and Back leave the `//` as typed. Years 1970–2099.
- **Decision (emoji)**: a fixed list of 48 common emojis in the bridge (`SlashBridge::emojiList()`),
  shown in a 6×8 grid; one tap inserts and closes. Host test: every code point has a glyph in the
  bundled emoji font (`PBNOTES_EMOJI_FONT`), so none renders as an empty box (feature 002's font).
  A single code point each (no ZWJ sequences, no skin tones, no variation selectors), so each is one
  tap and one character to delete.
- **[SPIKE S-4]**: on the InkPad, all 48 emojis show as glyphs (no boxes) and the steppers and grid
  cells are reliably hit with a finger. **Fallback**: drop failing emojis from the list; enlarge
  the cells.
- **Alternatives considered**: a text field with the date typed in (needs the keyboard); a calendar
  month grid (more taps for "today ± few days" than the steppers; a month grid is also harder to
  make legible in black and white); emoji search (keyboard again).

## R-12. Tests (constitution I)

Written first, observed failing, then implemented. New: `test_markdown_fence` (open/close rules,
tilde, lengths, indent, info string, unclosed, empty; attachments/tables/task toggle ignore the
same lines), `test_markdown_blocks` (headings 1–6, bold in each place, unmatched `**`, quote, info
kinds, details with and without summary/`open`, unclosed, nested, strike/underline incl. combined
with bold, links and table cells), `test_slash` (detect boundary cases, expand strings and carets,
new-line rule, every expansion re-parsed by the reader's parser, `format_timestamp`,
`days_in_month` incl. leap years, entry order). Extended: `test_markdown` (fence lines are no longer
paragraphs), `test_markdown_source_map` / `test_copy_source` (copy across a code block, a quote, a
callout, a collapsed section, partial selection inside code, inside a strike run),
`test_markdown_attachments`, `test_markdown_toggle`, `test_edit_history` (a group spanning several
observations, then replace → one undo step; dismissed group → ordinary step), `test_reader_blocks`
(new kinds and fields, `<s>`/`<u>` markup, escaping of `<details>` text, unsplittable code lines),
`test_edit_bridge` (`SlashBridge`, emoji glyph coverage), `test_edge_cases` (a note with every new
format saves and syncs byte for byte). QML page tests with injected mouse events:
`editor_slash.qml` (type `/` → menu, entry → text and caret, Undo reverses, `/` after a letter does
nothing, `//` → picker, cancel leaves `//`, Title field never opens a popup, Back closes only the
popup) and `reader_blocks.qml` (details tap opens and closes, hidden units skipped by markers, a
fenced block selected whole copies its fences, heading keep-with-next interval). `app_self_test`
must still pass. The device walkthrough in `quickstart.md` covers what only the panel shows.
