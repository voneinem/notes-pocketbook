# Research: Text Editing Tools and Title in the Reader

Resolves what the plan's Technical Context left open. Facts were read from this repository (core,
bridge, QML, tests, the SDK header `inkview.h`, features 001–004 specs and plans); nothing was
probed on the device. Items marked **[SPIKE]** need a short check on the InkPad before the work
that depends on them, and have a stated fallback.

## R-1. One shared history, built on diffs (FR-011 – FR-017)

- **Findings**: the editor has a one-line `TextField` (title), a `TextArea` (text), a `category`
  string and a `favorite` bool in `EditorPage.qml`; `dirty` compares all four with the values read
  at opening. Qt's `TextEdit.undo()/redo()` belong to one text document each.
- **Decision**: a Qt-free core class `EditHistory` (contracts/core-library.md) keeps the four
  current values and two stacks of *steps*. QML reports the new value of a field after each change
  (`onTextChanged`, a category choice, a favorite toggle); `observe()` diffs it against the known
  value (common prefix and suffix) and records one replacement `{pos, removed, inserted}` for text
  fields, or `{before, after}` for category and favorite. It therefore captures typing, a keyboard
  auto-correction, a swipe-typed word, an inserted attachment marker line (FR-016), a paste and a
  cut identically, without caring where the change came from. Undo/redo return what to apply and
  update the known values themselves, so the `onTextChanged` that follows finds no difference.
- **Origin**: `reset()` stores the opened values and clears both stacks. The history never holds a
  step that removes opened content: undo stops at the origin (FR-015). `dirty` stays a plain
  comparison with the opened values, so undoing everything makes it false again (FR-017) without
  new code.
- **Grouping (FR-012, SC-004)**: a single-character insertion continues the previous step when it is
  the same field, directly adjacent, no pause longer than 1.5 s, the step is under 80 characters,
  and neither side is a line break. Single-character backspace/forward-delete runs coalesce the same
  way. Anything larger (a pasted run, a multi-character replacement) is its own step. A 40-character
  sentence typed without a long pause is therefore one step (SC-004 allows 3 taps).
- **Compound commands**: `beginGroup(field)` / `endGroup()` make every observation in between one
  step (Cut = Copy + remove; Delete = remove; Paste over a selection = remove + insert). The step
  is the diff of the field's value at `beginGroup` against its value at `endGroup`.
- **Memory (FR-015)**: at most 200 steps and 2,000,000 stored characters; the oldest steps are
  dropped first, never the newest. Steps hold the removed/inserted fragments only, not whole texts,
  so a 100,000-character note costs little.
- **Alternatives considered**: Qt undo on the text area only (cannot order category/favorite or span
  title and text); whole-value snapshots per step (200 × 100,000 characters, too much memory);
  recording from key events (misses the on-screen keyboard's composed input and every programmatic
  insert); persisting the history (FR-015 says never).

## R-2. What undo restores: caret and selection (FR-012, FR-013)

- **Decision**: `undo()` of a step that *removed* text restores it and returns a selection over the
  restored range (the cut/deleted text "is back and selected", scenario 2 of US2); `undo()` of an
  insertion puts the caret at the insertion start; `redo()` puts the caret after the re-inserted
  text, or at the start of a removal. Category and favorite steps carry no caret; the editor keeps
  whichever field had focus.
- **Applying in QML**: `remove(pos, pos+len)` then `insert(pos, text)` on the `TextArea` /
  `TextField`, then `select()` / `cursorPosition`. Both operations raise `onTextChanged`, so the
  session has an `applying` flag: while it is set `observe()` ignores calls, and `finishApply(title,
  text)` clears it and re-syncs the known values with what the fields really hold (defensive: if
  they differ the UI value wins and the stacks are kept).
- **Alternatives considered**: assigning the whole `text` (relayouts 100,000 characters and loses
  the scroll position); a `Binding`-driven text property (cannot keep the caret).

> **Superseded in part (2026-10-11, after the first device test):** the tools row and the Extend button of R-3 and
> R-10 are gone. Selection starts with a **long press on a word** and is adjusted with **two draggable markers**; the
> commands are in a **bubble menu** next to the selection (like the reader's own, the author's screenshot). The gesture
> is read from `MouseArea` events (`pressAndHold`), not `TapHandler`, because mouse input is what the reader's touch
> screen is known to deliver (every button works that way); `SelectionHandle`, `SelectionHandles` and `SelectionMenu`
> are the new components, and the headless page tests inject real mouse events to cover the gestures.

## R-3. Starting a selection on e-ink (FR-001, FR-002, FR-009) — **[SPIKE S-1]**

- **Findings**: the on-screen keyboard is the reader's native one (rotation findings in project
  memory), so there is no hardware keyboard or shortcut. Qt Quick's `TextEdit`/`TextInput` offer
  `selectWord()`, `select(a, b)`, `selectAll()`, `positionAt()`, `selectionStart/End`,
  `selectedText`. On the reader, drag in the editor scrolls the text and the reading page has no
  drag at all (`interactive: false`), so drag-to-select would fight the page controls.
- **Decision**: three ways, none needing a drag:
  1. **Press-and-hold on a word** (`TapHandler` with `longPressThreshold` ≈ 0.6 s) selects it
     (`selectWord()` after the press placed the caret; in the reader via `positionAt`).
  2. **Extend**: the tools row's *Extend* button arms the next tap; the tap sets the other end of
     the range (anchor = the held word; the range runs from the word to the tapped point, in either
     direction). One arming, one tap; it disarms after the tap or on any other button.
  3. **All** (`selectAll()`; in the reader, every unit).
- **[SPIKE S-1] gate**: confirm on the InkPad that a held press produces a long-press in a
  `TapHandler` (the e-ink platform feeds touch as mouse/touch events; the existing code uses only
  `MouseArea`/`Button`). **Fallback** if it does not: a *Word* button in the tools row that selects
  the word at the caret (editor) or at the last tapped point (reader). The rest of the design is
  unaffected; Extend and All still work.
- **Alternatives considered**: selection handles (`Qt.TextEdit` handles are not part of the Basic
  style the app uses); a "select mode" toggle with two taps (works everywhere but costs three taps
  against SC-001's four); `selectByMouse` drag (conflicts with the editor's flick and is unusable
  with a finger on e-ink).

## R-4. The clipboard (FR-003 – FR-007, FR-032, FR-033) — **[SPIKE S-3]**

- **Findings**: `inkview.h` (SDK 6.11) has no clipboard function (a case-insensitive search for
  `clip` finds only the drawing clip-rectangle calls). The device Qt runtime's platform plugin is
  not ours; Qt's `QClipboard` works in-process on any platform and talks to other apps only if the
  plugin implements it. Qt Gui is already linked on the device.
- **Decision**: `ClipboardBridge` (contracts/qml-bridge.md) uses `QGuiApplication::clipboard()`
  when a `QGuiApplication` exists and keeps an in-app string otherwise, so Copy, Cut and Paste
  never fail because sharing is unavailable (spec edge case). Copy and Cut write it only on the
  reader's tap; Paste reads it only on a Paste tap and when updating the enabled state (`hasText`
  reflects `QClipboard::dataChanged`, never reads other content). Whatever comes from the platform
  clipboard is treated as untrusted (`clean_pasted_text`, R-5).
- **[SPIKE S-3]**: on the InkPad, check that `setText` then `text` round-trips inside the app and
  record whether `supportsSelection()`/the platform plugin shares it with another process. A
  negative result changes nothing in the design; it is recorded in the quickstart as the
  delivered behavior ("in-app clipboard", the spec's stated fallback).
- **Alternatives considered**: a file in the data directory as a poor-man's shared clipboard
  (writes note text to disk, violating FR-033); an InkView-based hook (none exists).

## R-5. Cleaning pasted text and the title rule (FR-008, FR-032)

- **Decision**: `clean_pasted_text(utf8, max_chars=1,000,000)` returns the text or an error key:
  invalid UTF-8 → `paste_invalid`; more than the cap → `paste_too_large`; otherwise removes control
  characters (U+0000–U+001F except `\n` and `\t`, U+007F, U+0080–U+009F), turns `\r\n`, `\r`, U+2028 and U+2029 into `\n` (Qt's
  `selectedText()` uses U+2029 for paragraph breaks, so the same function normalizes what Copy
  puts on the clipboard), and drops U+0000. `single_line(text)` turns every run of line breaks and
  tabs into one space for the title.
- **Title paste**: `ClipboardBridge.titlePaste(current, selBegin, selEnd)` builds the result
  (selection replaced, clipboard made single-line), checks the code-point count against 255 (the
  same limit `validate_title` applies at Save) and returns `{ok, text, caret}` or `title_too_long`
  with the field unchanged. Leading/trailing spaces are not trimmed while typing (trimmed at Save
  as today).
- **Alternatives considered**: refusing control characters instead of dropping them (a copied web
  snippet often holds stray ones; refusing would make Paste feel broken); truncating an over-long
  title (silently loses text, constitution VI).

## R-6. Reader title block (FR-019 – FR-025)

- **Findings**: `NotesBridge::openNote` returns `title = display_title(...)`, which substitutes
  "(untitled)" for an empty title; the list needs that placeholder, the reader must not show it
  (FR-024). The reader's body is a `Column` of block delegates inside a `Flickable`, so a leading
  item scrolls and pages with the content (FR-021).
- **Decision**: `openNote` adds `hasTitle` (trimmed stored title non-empty) and `titleText` (the
  stored title as is); `title` is unchanged for the list. A new `TitleBlock.qml` is the first item
  of the `Column`: a read-only selectable text (same machinery as a text block), bold,
  `fontPx × 1.7`, wrapping with no elision (FR-020), 24 px gap below; omitted entirely when
  `hasTitle` is false (no gap). The text blocks are untouched (FR-022: never de-duplicated, Q5).
  The editor shows no heading (FR-023: nothing to do, it never had one).
- **Alternatives considered**: a fixed header above the `Flickable` (would not scroll away and eats
  page height on every page, FR-021); reusing the heading style of level 1 (it follows body
  headings in size, so a body heading could look identical).

## R-7. Selectable text in the reader (FR-026, FR-029, FR-030) — **[SPIKE S-2]**

- **Findings**: `BlockText.qml` shows each block with a `Text` (`StyledText`, links via
  `onLinkActivated`). `Text` cannot show a selected range; `TextEdit { readOnly: true }` can
  (`select(a, b)`, `selectionColor`, `selectedTextColor`, `positionAt`, `selectWord`) and has
  `linkActivated`. Task boxes and attachments are separate items, not text.
- **Decision**: replace the `Text` in `BlockText` by a read-only `TextEdit` with
  `textFormat: TextEdit.RichText`, the same `html` the bridge builds (only `<b>`, `<i>`, `<u>`,
  `<a>` plus escapes, which RichText renders the same), `selectionColor: "#000000"`,
  `selectedTextColor: "#ffffff"`, no focus on press (`activeFocusOnPress: false`) so no keyboard
  opens. Tapping a link still calls `linkActivated` (unchanged behavior outside a selection,
  FR-029).
- **Units**: the selection is `(unitFrom, offsetFrom) .. (unitTo, offsetTo)` over the ordered units
  *title, block 0, block 1, …*. Text units (title, heading, paragraph, list item) take a character
  range; table rows, attachments and blank lines are *whole units* (selected or not, drawn with a
  thick black frame; Plan decision, see R-8). The page owns the selection; each unit gets
  `selFrom`/`selTo` (−1 = none) and draws itself.
- **Page and edge taps**: the two 50 px edge `MouseArea`s keep turning pages and do not touch the
  selection; taps in the body that are not Extend-armed and not on a link clear it (FR-030).
  `PageButtons` is untouched.
- **[SPIKE S-2]**: compare one note with a heading, bold/italic, a link, a task list and a table
  rendered by `Text` vs the read-only `TextEdit` on the InkPad: line breaks, heights and link
  taps must match, otherwise page breaks (`unsplittable`) shift. **Fallback** if parity fails:
  keep `Text` for display and select *whole blocks* only; the spec would then be amended to say
  partial selection is limited to the editor (a decision for the author, not made here).
- **Alternatives considered**: one big `TextEdit` for the whole note (loses per-block task boxes,
  tables, attachments and the page-break rules); painting selection over `Text` with
  `TextMetrics` (no per-character geometry for wrapped text).

## R-8. Table rows, attachments, blank lines (FR-026, FR-031)

- **Decision**: these are *whole units*. A selection that reaches them includes them entirely: a
  table row copies its source line, an attachment copies the marker line as written, a blank line
  copies an empty line. A cell cannot be partly selected. This is narrower than "any character
  range" in FR-026, which lists "paragraphs, headings and list items"; the narrowing is recorded
  as an edge case in the spec.
- **Why**: a cell is a separate `Text` inside a weighted row; char-level mapping across cells and
  pipes needs table-specific offsets for little gain.
- **Alternatives considered**: per-cell `TextEdit` and source offsets for tables (more parser work
  than the rest of the feature's parser change).

## R-9. Mapping a displayed range to source text (FR-031, Q2)

- **Findings**: `parse_markdown` gives one `Block` per source line (several per line when
  attachment references split it) with spans that already dropped the marks; `inline_spans` runs on
  the line *after* its prefix (`# `, `- `, `- [ ] `, indentation) was stripped. `Span.text` is the
  displayed text; nothing records where it came from.
- **Decision**: the parser records, per **byte** of `Span.text`, its byte index in the *source line*
  (`Span::src`, same size as `text`; for a character produced by a backslash escape, the index of
  the backslash); per span, the source extent of the formatted element it belongs to
  (`el_begin`/`el_end`, marks included; for plain text the characters' own extent); per block, the
  extent of the whole block in its line (`src_begin`/`src_end`, prefix included; for attachment
  blocks the reference as written). `parse_inline` takes a base offset so nested calls
  (emphasis inside links, links inside emphasis) report line-relative indexes.
- **Copy rule** (`copy_source`, core): for a selection from `(block a, char i)` to `(block b, char
  j)`:
  - a block selected completely, or reached from a previous block, contributes its whole source
    line extent (prefix marks included); a first or only block selected from inside contributes
    from `src[i]` (backslash included) to the end of the last selected character, widened to the
    extent `el_*` of every element whose characters are all selected;
  - a last block selected from its start contributes its prefix and then up to the last selected
    character, widened the same way; blocks are joined by `\n`; a blank block is an empty line;
  - the title unit contributes its characters only; if the selection continues into the text a
    `\n` follows it.
  - A partly selected element never gets its marks (US4-10); a completely selected element always
    does (US4-9), including a Markdown link `[label](address)` (spec edge case).
- **Alternatives considered**: greedy alignment of displayed against source text (see Complexity
  Tracking); copying the whole source line(s) of every block touched (violates US4-10); copying
  displayed text (rejected by the Q2 answer).

## R-10. Layout and focus of the tools (FR-010, FR-011, FR-027)

- **Findings**: `EditorPage`'s `top` column holds the button row, Title, Category, notes and the
  rule, all above the text `Flickable`, which ends above the keyboard. A `Button` takes active
  focus on press, which would drop the field's focus and hide the keyboard.
- **Decision**: a new `EditTools.qml` row directly under the button row, buttons with
  `focusPolicy: Qt.NoFocus`, font 28, padding 14: *Undo Redo All Extend Copy Cut Paste Del*
  (about 780 px, fits the 1020 px width of the portrait page; in landscape it fits in one row
  too). Because the row is in the top column it is never behind the keyboard. The page keeps
  `activeField` (`"title"` or `"text"`) from `activeFocusChanged`, and the commands act on it.
  Enabled states: Copy/Cut/Del need a selection in the active field, Paste needs `clipboard.hasText`,
  Undo/Redo need `editSession.canUndo/canRedo`, Extend needs a selection.
- **Reader**: no tools row at the top (keeps page height). Select all, Extend and Copy go in the
  footer beside the page buttons (always visible, Copy disabled without a selection), and the
  header row is unchanged.
- **Vertical budget (portrait 1080×1335)**: the editor header grows by one 78 px row; with the
  keyboard open the text area keeps about 240 px, still four lines at the default size. Accepted;
  the walkthrough checks it.
- **Alternatives considered**: a popup toolbar near the selection (focus handling and e-ink
  redraws); a second tools row (costs another 78 px); icon-only buttons (no icon set in the app).

## R-11. Reload and font size end the reader selection (FR-030)

- **Decision**: `ReaderPage.load()` (already called after an attachment change, a task toggle or
  `StackView.onActivated`) clears the selection; `bridge.fontSize` changes clear it; the footer's
  Back and leaving the page drop it naturally. Page turns do not (FR-030 as corrected in the
  spec: a range is extended across pages with Extend plus a page button).
- **Spec note**: the first draft said "turning the page ends the selection" and, elsewhere, that the
  range can be extended after turning the page; planning removed the contradiction in favor of the
  second (US4-6 and FR-030 corrected).

## R-12. Tests (constitution I) and what the QML covers

Written first, observed failing, then implemented. New files are in **bold**.

| Test | Layer | Covers |
|------|-------|--------|
| **test_edit_history** | host unit | diff steps for insert/delete/replace; typing grouping (pause, 80-char cap, line break); backspace runs; `beginGroup`/`endGroup` as one step; undo/redo order across title, text, category, favorite (FR-013); redo cleared by a new edit (FR-014); origin never undone (FR-015); caps drop oldest, keep newest; `applying` suppression; surrogate pairs are not split |
| **test_text_clean** | host unit | control characters dropped, `\n`/`\t` kept, CR/LF/U+2028/2029 normalized, invalid UTF-8 refused, 1,000,000 cap, `single_line` (FR-008, FR-032) |
| **test_markdown_source_map** | host unit | `Span::src`, `el_*`, `Block::src_*` for plain text, headings, list/task prefixes, bold/italic/both, nested emphasis+link, `[label](url)`, `<autolink>`, bare address, `\*` escapes, inline code, text after an attachment reference, multibyte UTF-8; existing parser tests unchanged |
| **test_copy_source** | host unit | US4-8/9/10 and FR-031: whole and partial blocks, completely vs partly selected elements, link copied as written, block prefix rule, blank lines, table rows and attachments as whole units, title unit, selection across blocks and in reverse order |
| **test_edit_bridge** | bridge | `ClipboardBridge` (in-app fallback, cleaning, `hasText`, `cleanPaste`, `titlePaste`, offscreen `QClipboard` when available) and `EditSession` (QString conversion, signals, clock seam) |
| test_reader_blocks (+) | bridge | each entry carries `block`, `row`, `src` fields consistent with the parse |
| test_notes_bridge (+) | bridge | `openNote` `hasTitle`/`titleText` (empty and whitespace titles; list keeps "(untitled)"); `readerCopy` leaves the note, its modified time and its pending state unchanged (SC-010) |
| app `--self-test` | app | the editor and reader pages still load (QML syntax/imports) |
| device walkthrough | manual | gestures, tools row with keyboard, rendering parity, page turns with a selection, rotation (quickstart §4) |

## R-13. Version and release (constitution)

- **Decision**: `VERSION` 1.1.0 → **1.2.0** (a feature), with a CHANGELOG entry; last phase of
  `tasks.md`; `scripts/check-version-bump.sh` enforces it in `scripts/pre-merge.sh`. No data
  upgrade, so no MAJOR.
