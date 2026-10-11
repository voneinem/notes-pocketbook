# Feature Specification: Better Markdown Support and Slash Menu

**Feature Branch**: `feature/006-markdown-slash-menu`

**Created**: 2026-10-11

**Status**: Draft

**Input**: User description: "Better markdown support: Support for code blocks (e.g. ```yaml ...), Headers (#, ##, ###), bold (** **). In edit mode invoke template selection via "/" to show a dropdown with bullet list, table, info textbox, link, bold, cursive, underline, strikethrough, details, quote, codeblock, Emoji. "//" shall bring up a date time picker"

## Clarifications

### Session 2026-10-11

- Q: Should the reading page display quotes, info boxes, strikethrough, underline and collapsible details as formatted text, or only insert their Markdown and show it as typed? → A: Render all five (User Story 4, FR-016 to FR-018 stand as written).
- Q: In what form should the date-time picker write the chosen moment into the note? → A: `YYYY-MM-DD HH:MM`, 24-hour, fixed for everyone (FR-014 stands as written).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Read code blocks in notes (Priority: P1)

A reader keeps snippets in their notes: a configuration file, a shell command, a few lines of
code, written as a fenced block (three backticks, an optional language name such as `yaml`, the
lines, three backticks). Today these show up as ordinary running text, with the fence lines
visible and the indentation lost. The reader now sees the block as a clearly separate area in
fixed-width type, with every line and every space exactly as written.

**Why this priority**: Code and configuration are the content that is damaged most by ordinary
text layout (indentation is meaning in YAML), and a fenced block is the most common thing
people paste into notes. It is also the one block in the request that does not work at all today.

**Independent Test**: Open a note containing a fenced block labelled `yaml` with three nested,
indented lines, and a line inside it that looks like `**not bold**` and `# not a heading`. Verify
the block is shown as a distinct area in fixed-width type, the indentation is intact, the
fence lines are not shown as text, and the asterisks and hash are shown literally.

**Acceptance Scenarios**:

1. **Given** a note contains a fenced block (opening and closing fence of three backticks, with or
   without a language name after the opening fence), **When** the reader opens it, **Then** the
   lines between the fences are shown as one block that is visibly set apart from the
   surrounding text (a border or shaded background that stays legible in black and white) and set
   in fixed-width type.
2. **Given** a line inside the block starts with spaces or tabs, **When** the block is shown,
   **Then** the indentation is preserved, and runs of spaces inside a line are not collapsed.
3. **Given** a block contains text that would be formatting elsewhere (`# heading`, `**bold**`,
   `- item`, `[a](http://b.example)`, a pipe table, an emoji shortcode), **When** the block is
   shown, **Then** it is shown literally, with no formatting applied and no link that can be
   followed.
4. **Given** a line inside a block is wider than the screen, **When** the note is opened, **Then**
   the line wraps onto the next row of the block; nothing is reachable only by sideways scrolling.
5. **Given** a block is long, **When** the reader pages through the note, **Then** the block may
   continue on the next page, but a line is never split across two pages.
6. **Given** an opening fence is never closed, **When** the note is shown, **Then** the block runs
   to the end of the note and nothing in it is lost.
7. **Given** a block has a language name after the opening fence, **When** it is shown, **Then**
   the name is not shown as part of the code (no syntax coloring is applied).
8. **Given** the reader selects text on the reading page that includes a code block and copies it,
   **When** they paste it, **Then** the copy contains the fence lines and the code exactly as
   written in the note.
9. **Given** the reader edits a note with a code block and saves, **When** they look at the text,
   **Then** the fenced text is unchanged byte for byte; and a fenced block that was written with
   four or more backticks or with tildes (`~~~`) is also shown as a block, with the closing fence
   matching the opening one.
10. **Given** an empty block (fences with nothing between them), **When** the note is shown,
    **Then** a small empty block is shown and no error occurs.

---

### User Story 2 - Clearly visible headings and bold text (Priority: P1)

A reader opens a note that uses `#`, `##` and `###` headings and `**bold**` phrases. They can
tell at a glance which line is a heading and how important it is: a first-level heading is larger
than a second-level one, which is larger than a third-level one, and all are larger or heavier
than the body text. Bold phrases are clearly heavier than the surrounding text, wherever they
appear: in a paragraph, a heading, a list item, a table cell.

**Why this priority**: Headings and bold are the structure of most notes. They are partly
displayed today; on the e-ink screen the differences between levels and between bold and normal
text must be unmistakable, otherwise a note reads as a wall of text. It belongs with the code
block as the "reading" half of the request.

**Independent Test**: Open a note with `#`, `##`, `###`, `####` headings, a bold phrase in a
paragraph, in a heading, in a list item and in a table cell, plus a stray `**` that never closes.
Verify four visibly different heading sizes in descending order, bold visibly heavier than body
text in every place, and the stray asterisks shown as typed.

**Acceptance Scenarios**:

1. **Given** a note has lines starting with `# `, `## ` and `### ` (hash marks, a space, then
   text), **When** the reader opens it, **Then** each is shown as a heading without its hash
   marks, and the three levels have three visibly different sizes in descending order.
2. **Given** a heading of level four to six, **When** it is shown, **Then** it is still shown as a
   heading (not smaller than, and visibly different from, body text) without its hash marks.
3. **Given** a line starts with hash marks but no space after them (for example `#hashtag`),
   **When** the note is shown, **Then** it is ordinary text with the characters as typed.
4. **Given** a phrase is wrapped in `**` on both sides, **When** it is shown, **Then** it is
   visibly bolder than the surrounding text and the asterisks are not shown; this holds in a
   paragraph, a heading, a list item, a task item and a table cell.
5. **Given** a `**` that has no matching closing `**` in the same line, **When** the note is
   shown, **Then** the asterisks are shown as typed and nothing after them is made bold.
6. **Given** the reader changes the reading font size, **When** the note is shown, **Then**
   headings keep their size relation to each other and to the body text.
7. **Given** a heading is the last line on a page, **When** the reader pages through the note,
   **Then** the heading is not left alone at the bottom of a page, separated from the text it
   introduces.
8. **Given** a heading or bold phrase lies inside a fenced code block, **When** the note is shown,
   **Then** it is shown literally (see User Story 1).

---

### User Story 3 - Insert formatting from a "/" menu in the editor (Priority: P1)

A reader writing a note on the on-screen keyboard does not want to hunt for symbols such as
`` ` ``, `|`, `~` or `[`. While editing, they type "/" and a dropdown list appears next to the
caret offering: bullet list, table, info box, link, bold, italic, underline, strikethrough,
details, quote, code block, emoji. They tap one and the matching Markdown is inserted at the
caret, with the caret placed where they continue typing.

**Why this priority**: Typing Markdown syntax on an e-ink keyboard is slow and error-prone; this
menu is what makes writing the formats of User Stories 1, 2 and 4 practical. It is useful on its
own even before every format has special display, because the inserted text is plain Markdown.

**Independent Test**: In the editor, on an empty line type "/", verify the dropdown with the
twelve entries appears. Choose "Code block": the text now contains an opening fence, an empty
line and a closing fence, with the caret on the empty line, and the "/" is gone. Press Undo: the
note is as before the insertion.

**Acceptance Scenarios**:

1. **Given** the editor is open, **When** the reader types "/" at the start of a line or after a
   space or line break, **Then** a dropdown list opens next to the caret with these entries in this order:
   bullet list, table, info box, link, bold, italic, underline, strikethrough, details, quote,
   code block, emoji.
2. **Given** the "/" is typed directly after a letter, digit or punctuation (for example in a web
   address, a date `1/2`, `and/or`, or `http://`), **When** the reader continues, **Then** no
   dropdown appears and the "/" is an ordinary character.
3. **Given** the dropdown is open, **When** the reader taps an entry, **Then** the "/" is replaced
   by the entry's Markdown, the dropdown closes, and the caret is placed at the first position where text is
   expected, according to the table below.
4. **Given** the dropdown is open, **When** the reader types any other character, taps outside the
   dropdown, moves the caret, or leaves the field, **Then** the dropdown closes and the "/" and
   anything typed stay as ordinary text.
5. **Given** the dropdown is open, **When** the reader presses the device's Back control, **Then**
   only the dropdown closes (the editor stays open and the "/" stays).
6. **Given** an insertion was made from the menu, **When** the reader taps Undo, **Then** the whole
   insertion is reversed in one step, leaving the text as it was before the "/" was typed; Redo
   inserts it again.
7. **Given** the caret is in the Title field, **When** the reader types "/", **Then** no dropdown
   appears (a title is one line of plain text).
8. **Given** the editor has a text selection, **When** the reader types "/", **Then** the selection
   is replaced by the "/" as for any character and the dropdown opens at the caret.
9. **Given** the dropdown is open near the lower edge of the screen or with the on-screen keyboard
   shown, **When** it is displayed, **Then** it is fully visible, never covered by the keyboard,
   and every entry can be tapped; if it does not fit, it scrolls.
10. **Given** the insertion places a block (bullet list, table, info box, details, quote, code
    block) in the middle of a line, **When** it is inserted, **Then** it starts on a new line
    (a line break is added if the "/" was not at the start of a line), so the inserted Markdown is valid.

What each entry inserts (the caret is shown as `|`):

| Entry | Inserted text | Caret ends |
|-------|---------------|------------|
| Bullet list | `- ` | after the dash and space |
| Table | a header row of two columns, the separator row and one empty body row | in the first header cell |
| Info box | a quote line starting `> [!NOTE]` followed by a quote line `> ` | after `> ` on the second line |
| Link | `[](https://)` | between the brackets |
| Bold | `****` | between the two pairs of asterisks |
| Italic | `**` (single asterisks) | between them |
| Underline | `<u></u>` | between the tags |
| Strikethrough | `~~~~` | between the two pairs of tildes |
| Details | `<details>`, a `<summary>` line, an empty line, `</details>` | inside the summary |
| Quote | `> ` | after the marker |
| Code block | fence line, empty line, fence line | on the empty line |
| Emoji | opens the emoji picker (User Story 5) and inserts the chosen emoji | after the emoji |

**Why the table's exact texts are fixed**: each is the same Markdown the reading page displays
(User Stories 1, 2 and 4), so what is inserted is what is shown.

---

### User Story 4 - Read the other formats the menu inserts (Priority: P2)

A reader whose notes contain quotes, info boxes, strikethrough, underlined phrases and
collapsible details sees them formatted rather than as raw symbols: a quote is set off with a
bar at the left, an info box stands out as a labelled box, struck-through text is crossed out,
underlined text is underlined, and a details section shows only its summary line until the reader
taps it to open it.

**Why this priority**: Without it, the menu of User Story 3 would insert syntax that the reader
then shows as clutter. It is lower than the first three because the note text stays complete and
readable either way, and the raw form is what other apps show for these on some platforms.

**Independent Test**: Open a note with a quote, an info box, `~~struck~~` text, an underlined
phrase and a details section. Verify each is displayed in its form; tap the details summary to
expand and collapse it.

**Acceptance Scenarios**:

1. **Given** lines starting with `> `, **When** the note is shown, **Then** they are shown
   as one quote: indented with a vertical bar at the left, without the `> ` marks; consecutive quote lines form one quote.
2. **Given** a quote whose first line is `> [!NOTE]` (also `[!TIP]`, `[!WARNING]`, `[!IMPORTANT]`,
   `[!CAUTION]`), **When** the note is shown, **Then** it is shown as an info box: a bordered box
   with a label naming the kind (Note, Tip, Warning, Important, Caution) and the rest of the
   quote as its content; the label does not rely on color. Other quotes are plain quotes.
3. **Given** text wrapped in `~~` on both sides, **When** shown, **Then** it is crossed out and the
   tildes are not shown; an unclosed `~~` is shown as typed.
4. **Given** text wrapped in `<u>` and `</u>`, **When** shown, **Then** it is underlined and the
   tags are not shown; an unclosed `<u>` is shown as typed. Underlined text that is a link is
   still recognizable as a link (see Edge Cases).
5. **Given** a `<details>` section with a `<summary>` line, **When** the note is opened, **Then**
   only the summary is shown, with a clear marker that it can be opened; tapping it shows the
   content below it, tapping again hides it. The initial state is closed (open if the author wrote `<details open>`).
6. **Given** a details section contains text, a list or a code block, **When** it is expanded,
   **Then** that content is displayed with the same formatting as elsewhere.
7. **Given** quotes, info boxes, strikethrough or underline appear inside a table cell, **When**
   shown, **Then** inline formats (strikethrough, underline, bold, italic) are displayed in the cell;
   block formats (quote, info box, details) are shown as typed.
8. **Given** the reader edits and saves a note containing any of these, **When** they look at the
   text, **Then** it is unchanged apart from their edit.
9. **Given** the reader selects and copies text on the reading page that includes these formats,
   **When** they paste it, **Then** the copy is the source text with its marks, as in feature 005.
10. **Given** a details section is not closed (`</details>` missing), **When** the note is shown,
    **Then** the section runs to the end of the note and nothing is lost.

---

### User Story 5 - Insert a date and time, and emojis, from pickers (Priority: P2)

A reader writing a meeting note or a journal entry types "//" and a small date-and-time picker
appears. They choose a date and a time (it starts at now), confirm, and the chosen date and time
are inserted as text at the caret. From the "/" menu, choosing Emoji opens a compact grid of
common emojis; tapping one inserts it.

**Why this priority**: Typing dates and finding emojis on an e-ink keyboard is slow, but notes
are fully usable without it, and the pickers add no new content type, only convenience.

**Independent Test**: In the editor type "//" on an empty line. Verify the picker opens showing
today's date and the current time; change the day and the hour, confirm, and verify the text
`YYYY-MM-DD HH:MM` for the chosen moment is in the note and the "//" is gone. Undo restores the
note. Then type "/", choose Emoji, tap one, and verify it is inserted.

**Acceptance Scenarios**:

1. **Given** the editor is open, **When** the reader types a second "/" directly after the "/"
   that opened the menu (so "//" at the start of a line or after a space), **Then** the dropdown
   closes and a date-time picker opens instead.
2. **Given** the picker is open, **When** it appears, **Then** it shows the current date and time
   preselected, and the reader can change the year, month, day, hour and minute using on-screen controls
   large enough to tap reliably, without the keyboard.
3. **Given** the picker is open, **When** the reader taps Insert (confirm), **Then** the "//" is
   replaced by the chosen moment written as `YYYY-MM-DD HH:MM` (24-hour), the picker closes and
   the caret is after the inserted text.
4. **Given** the picker is open, **When** the reader taps Cancel or presses the device's Back
   control, **Then** the picker closes and the "//" stays as typed text.
5. **Given** the picker is open, **When** the reader selects a date that does not exist (for
   example 31 February), **Then** it cannot be selected: the day choices only offer days of the chosen
   month and year.
6. **Given** a date-time was inserted, **When** the reader taps Undo, **Then** the insertion is
   reversed in one step, back to the text before "//" was typed.
7. **Given** the caret is in the Title field, **When** the reader types "//", **Then** no picker
   appears and the characters are ordinary text.
8. **Given** the "/" is typed after a letter or punctuation (for example `https://`), **When** the
   reader types a second "/", **Then** no menu or picker appears.
9. **Given** the reader chooses Emoji in the dropdown, **When** the emoji grid appears, **Then** it
   shows common emojis in a grid, each large enough to tap, and tapping one replaces the "/" with it
   and closes the grid; Cancel or Back closes the grid and leaves the "/".
10. **Given** an emoji or a date was inserted, **When** the note is saved and synced, **Then** it
    appears unchanged on the reading page and on other devices (emoji handling of feature 002).

---

### Edge Cases

- The "/" menu is opened and the note is rotated or the keyboard is hidden: the menu closes without
  changing the text.
- Another feature's selection menu (long press, feature 005) is open: typing "/" closes it first,
  as typing any character does, and then the slash menu opens.
- A note contains a very long unbroken line inside a code block (for example a hash of 500
  characters): it wraps at the screen edge, no text is lost, and the page does not become unusable.
- A code fence is written inside a quote or a details section: it is displayed as a code block
  inside that element. A fence indented under a list item is shown as ordinary text (list items are
  one line each in this app).
- A line has up to three spaces before the fence marks: it is still a fence; four or more spaces
  make it ordinary indented text.
- A closing fence of fewer backticks than the opening one does not close the block.
- A table contains a `|` inside a code span or block: the existing table rules apply unchanged.
- The reader types "/" in the middle of existing text after a space (for example `a / b`): the
  dropdown opens; typing a space or any character, or tapping elsewhere, closes it and leaves
  the text as typed. Typing "/" is never blocked and never loses text.
- Hardware-keyboard or paste input: pasted text containing "/" never opens the menu or picker;
  only a "/" typed by the reader does.
- The menu, the date-time picker and the emoji grid follow the device's current interface language
  for their labels and the system's date and time zone for "now".
- A note written elsewhere uses the same formats with minor variants (`__bold__`, `_italic_`,
  `* ` bullets): these keep displaying as they do before this feature; this feature changes the
  display of `**`, `#` headings and the formats listed, nothing else.
- A link text contains underline or strikethrough marks: the link stays followable with the
  existing confirmation rules of feature 002.
- Nested details sections: the inner one is shown as typed inside the outer one's content.
- The list of notes and the note title show titles and previews without block formatting, as
  today; none of the new formats changes the list.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The reading page MUST show a fenced block (three or more backticks, or three or more
  tildes, optionally preceded by up to three spaces, with an optional language name after the opening
  fence) as one block set apart from the text, in fixed-width type, with indentation and spacing of
  every line preserved.
- **FR-002**: Text inside a fenced block MUST be shown literally: no heading, bold, list, link,
  table or other formatting is applied and no link inside it can be followed.
- **FR-003**: A fenced block MUST be closed by a fence of the same kind with at least as many
  marks as the opening fence; an unclosed block MUST run to the end of the note. The fence lines
  and the language name MUST NOT be shown as text, and no syntax coloring is applied.
- **FR-004**: Long lines in a fenced block MUST wrap within the block (no sideways scrolling), and
  paging MUST NOT split a line across pages.
- **FR-005**: The reading page MUST show lines starting with one to six hash marks and a space as
  headings (without the hash marks) with three visibly distinct sizes for levels one, two and
  three in descending order, and levels four to six at least distinct from body text. Hash marks
  without a following space MUST stay ordinary text. A heading MUST NOT be the last item on a page
  unless it is the last item of the note.
- **FR-006**: The reading page MUST show text wrapped in `**` as visibly bolder than body text
  (without the asterisks) in paragraphs, headings, list items, task items and table cells; an
  unmatched `**` MUST be shown as typed.
- **FR-007**: In the editor, typing "/" in the note text at the start of a line or directly after
  a space or line break MUST open a dropdown list with the entries, in this order: bullet list,
  table, info box, link, bold, italic, underline, strikethrough, details, quote, code block, emoji.
  A "/" typed directly after any other character MUST NOT open it.
- **FR-008**: Choosing an entry MUST replace the typed "/" with the Markdown given for that entry in
  User Story 3 and place the caret as specified there. Block entries (bullet list, table, info
  box, details, quote, code block) MUST start on a new line, adding a line break if needed.
- **FR-009**: The dropdown MUST close, leaving the text as typed, when the reader types any other
  character, taps outside it, moves the caret, leaves the field, rotates the device, or presses Back
  (Back closes only the dropdown). It MUST be fully visible and tappable with the on-screen keyboard
  shown.
- **FR-010**: Neither the dropdown nor the date-time picker MUST appear in the Title field, or for
  pasted text.
- **FR-011**: Each insertion from the dropdown, the emoji grid or the date-time picker MUST be one
  step in the editor's Undo/Redo history of feature 005, reversing completely to the text before
  the "/" or "//" was typed.
- **FR-012**: Typing a second "/" directly after the "/" that opened the dropdown MUST close the
  dropdown and open a date-time picker.
- **FR-013**: The date-time picker MUST start at the current date and time, let the reader change
  year, month, day, hour and minute without the on-screen keyboard, offer only days that exist in
  the chosen month and year, and use a 24-hour clock.
- **FR-014**: Confirming the picker MUST replace the "//" with the chosen moment written as
  `YYYY-MM-DD HH:MM`; cancelling or Back MUST close it and leave the "//" as typed.
- **FR-015**: The Emoji entry MUST open a grid of commonly used emojis; tapping one MUST replace the
  "/" with it; cancelling MUST leave the "/".
- **FR-016**: The reading page MUST show quote lines (`> `) as a quote with a vertical bar, without
  the marks, and a quote whose first line is `[!NOTE]`, `[!TIP]`, `[!WARNING]`, `[!IMPORTANT]` or
  `[!CAUTION]` as a bordered info box with a text label naming its kind (not relying on color).
- **FR-017**: The reading page MUST show text wrapped in `~~` as crossed out and text wrapped in
  `<u>`...`</u>` as underlined, without the marks; unmatched marks MUST be shown as typed.
- **FR-018**: The reading page MUST show a `<details>` section with a `<summary>` as its summary line
  only, closed unless written `<details open>`, which the reader can open and close by tapping the
  summary; its content MUST use the same formatting as elsewhere. An unclosed section MUST run to
  the end of the note.
- **FR-019**: The note's text MUST NOT be modified by displaying any of these formats; saving a note
  MUST preserve the text exactly as typed, and copying on the reading page MUST return the source
  text with its marks (feature 005).
- **FR-020**: The new display formats MUST NOT change how notes are stored or synced, nor how
  attachments, tables, task checkboxes, links and emojis (features 001–005) are displayed or
  behave, except that these are displayed inside quotes, details sections and fenced blocks as
  specified above.
- **FR-021**: Everything in this feature MUST work offline and without any network access, and MUST
  NOT send any note content anywhere.

### Key Entities

- **Fenced block**: a run of lines between an opening and a closing fence, with an optional
  language name, displayed literally in fixed-width type.
- **Heading**: a line with one to six hash marks and a text; has a level that sets its size.
- **Quote / info box**: one or more quote lines; an info box is a quote that begins with a kind
  marker (note, tip, warning, important, caution).
- **Details section**: a summary line and hidden content that the reader can open and close.
- **Slash menu entry**: a named, fixed piece of Markdown with a defined caret position, offered
  after typing "/".
- **Date-time stamp**: a chosen moment inserted as plain text in the form `YYYY-MM-DD HH:MM`.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A reader can insert any of the twelve formats into a note with at most three taps
  (type "/", tap the entry, and for emoji tap one emoji) and without typing any of the Markdown
  symbols themselves.
- **SC-002**: A reader can insert the current date and time into a note in at most three taps
  after typing "//" (open, confirm), and any other moment in at most ten.
- **SC-003**: A code block with nested indentation is displayed with its indentation and all
  characters identical to the note's text in 100% of the test notes used in verification.
- **SC-004**: In a test note with all heading levels, a reader can identify the level order
  (largest to smallest) correctly at normal reading distance on the device, and every bold phrase
  is distinguishable from body text.
- **SC-005**: Typing "/" in an ordinary context (addresses, dates such as 1/2, and/or) causes the
  menu to open in 0 of the test cases, and when it opens by mistake it can be dismissed with one tap
  without changing the typed text.
- **SC-006**: No note text is lost or altered in a verification run of 20 notes mixing all new
  formats, opened, edited elsewhere and saved, and synced: texts before and after are identical
  except for the reader's own edits.
- **SC-007**: Every insertion from the menu or pickers is reversed completely by one tap of Undo
  in 100% of the test cases.
- **SC-008**: Opening and paging a note with 500 lines of code in blocks stays as responsive as
  paging an ordinary note of the same length on the device.

## Assumptions

- The twelve menu entries are exactly as listed in the request; "cursive" means italic and "info
  textbox" means an info box (callout) in the style used by common Markdown tools; "details"
  means a collapsible section.
- The dropdown opens only for a "/" typed after whitespace or at a line start, to keep addresses,
  fractions and "and/or" unaffected; typing any further character simply closes it (no
  type-to-filter).
- Markdown for formats Markdown lacks follows common practice: underline as `<u>...</u>`, details as
  `<details><summary>...</summary>...</details>`, info box as a quote starting `[!NOTE]`
  etc. Other apps may show this syntax raw; this is accepted.
- The display is monochrome-friendly: no syntax coloring, and info box kinds are told apart by a text
  label.
- "Italic" is written with single asterisks and "bold" with double asterisks, as already read by
  the app today; other variants keep their current display (see Edge Cases).
- The date-time format `YYYY-MM-DD HH:MM` (24-hour, device-local time) is fixed; no setting
  for another format is offered.
- In the date-time picker the minute moves in steps of five (the initial value is the exact current minute); the other
  columns move by one.
- The emoji grid shows a fixed set of common emojis that the device can display; searching and
  skin-tone selection are out of scope.
- Inline code spans (single backticks), code syntax highlighting, nested quote levels, and editing
  these formats directly on the reading page are out of scope; the reading page remains read-only.
- Depends on feature 005 (editor with undo/redo and selection) and feature 002 (emoji, links,
  tables); the heading and bold text parsing already present is extended, not replaced.
