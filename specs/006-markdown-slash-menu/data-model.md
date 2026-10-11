# Data Model: Better Markdown Support and Slash Menu

**Nothing is stored.** No table, column, setting, file or sync field changes; the schema stays at the
version of feature 005. Notes remain plain Markdown text, stored and synced byte for byte. Everything
below lives in memory while a note is parsed for the reading page or the editor is open.

## Parse result (core, `markdown.h`)

### BlockKind (additions)

| Kind | Source line | Shown as | Selected as |
|------|-------------|----------|-------------|
| `Fence` | an opening or closing fence line | the top / bottom edge of the code box (no text) | whole unit |
| `Code` | a line between the fences (or to the end of the note when unclosed) | one line of fixed-width literal text | text unit (character range) |
| `Callout` | `> [!NOTE]` etc. | the info box header: the kind as text | whole unit |
| `Summary` | `<summary>text</summary>`, or the `<details>` line when no summary follows | the toggle row `▶/▼ text` | text unit |
| `Marker` | `<details>` (before a summary line) and `</details>` | nothing (height 0) | whole unit, never hit-tested |

Existing kinds (`Heading`, `Paragraph`, `ListItem`, `Blank`, `Table`, `Attachment`) are unchanged.

### Block (additions)

| Field | Meaning |
|-------|---------|
| `quote` | `None`, `Quote` or `Info` — the block is inside a quote / info box (any block kind can be) |
| `callout` | `Note`, `Tip`, `Warning`, `Important`, `Caution` — on `Callout` blocks |
| `quote_first`, `quote_last` | first / last block of its quote group (post-pass) |
| `code_first`, `code_last` | first / last block of its code box (post-pass); the last `Code` block of an unclosed fence is `code_last` |
| `details_id` | −1, or the 0-based source line of the `<details>` line of the section the block belongs to (the `Summary` and the content blocks; not the `Marker`s) |
| `details_open` | the section was written `<details open>` (initial state) |
| `fence_info` | the info string after an opening fence (tests only, never displayed) |

`src_begin` / `src_end` follow feature 005: from column 0 of the source line (a quote's `> ` prefix
included) to the end of the block's text; a `Summary` extends to the end of the line so a whole
selection copies the `<summary>` tags; `Fence`, `Callout`, `Marker` cover the whole line.

### Span (additions)

`strike`, `underline` (booleans, default false). Spans merge only when kind, link and both flags are
equal. Source mapping (`src`, `el_begin`, `el_end`) is recorded as for `**`.

### Rules

- **Fence** (shared helper): opener = ≤ 3 spaces of indent (a tab counts 4), then three or more of
  one character, `` ` `` or `~`; the info string of a backtick opener must not contain a backtick.
  Closer = same character, at least as many, only spaces after. Inside a quote the same rule is
  applied to the line after the quote marker; a fence opened inside a quote ends at its closer or
  when the quote ends.
- **Quote**: ≤ 3 spaces of indent, `>`, optional single space; the remainder is parsed as a line.
  A run of quote lines is one group; a run whose first line is `[!KIND]` is an info box.
- **Details**: top level only; nested `<details>` lines are ordinary text; unclosed runs to the end.
- **Inline**: `~~x~~`, `<u>x</u>` as in research R-5; unmatched marks are literal.
- **Lines longer than 16,384 bytes** stay plain text (existing rule): no quote, details or inline
  formats, but a `Code` line is shown literally in full.

## Reader block (bridge → QML, per block)

New `kind` values `"fence"`, `"code"`, `"callout"`, `"summary"`, `"marker"`; new fields `quote`
(0/1/2), `quoteFirst`, `quoteLast`, `codeFirst`, `codeLast`, `callout` (`"note"` …), `detailsId`,
`detailsOpen`, and `unsplittable: true` on `code` and `fence` blocks. Text of every kind is escaped
to HTML by the bridge; `<s>` and `<u>` are generated from span flags. Nothing from the note is ever
inserted as markup.

## Reading-page state (QML, memory only)

| State | Meaning |
|-------|---------|
| `openDetails` | `{id: bool}` — the sections the reader opened or closed this visit; absent = the block's `detailsOpen`. Survives reloads of the same note; dropped when the page closes |

## Editor state (QML, memory only)

| State | Meaning |
|-------|---------|
| `previousText` | the area's text before the latest change (input of `detect`) |
| `programmatic` | set while the editor changes the text itself (paste, undo/redo, token insert, menu insertion) |
| `slashState` | `{pos, kind}` while a popup is open: `kind` = `menu`, `datetime` or `emoji`; null otherwise |
| the open history group | started with the first `/` of a trigger, ended by choosing, dismissing or any other command |

## Slash menu entries (core, `slash.h`)

Twelve fixed entries, in this order, each with an id, a label and the replacement given in research
R-9: `bullet_list`, `table`, `info_box`, `link`, `bold`, `italic`, `underline`, `strikethrough`,
`details`, `quote`, `code_block`, `emoji`. Labels follow the interface language like the rest of the
app. No setting, no ordering by use.

## Date-time stamp

A moment in device-local time, written `YYYY-MM-DD HH:MM` (zero-padded, 24-hour). Valid: year
1970–2099, month 1–12, day 1–`days_in_month`, hour 0–23, minute 0–59. The picker clamps rather than
rejects. The text is inserted as plain text; nothing parses it later.

## Emoji grid

48 single-code-point emojis, fixed, in the bridge; every entry has a glyph in the bundled emoji font
(tested).

## Unchanged

Note, category, attachment, account, sync state and the editor history's data model (feature 005).
The only new use of the history is a group opened for the typed `/` (research R-8).
