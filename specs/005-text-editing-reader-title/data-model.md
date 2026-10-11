# Data Model: Text Editing Tools and Title in the Reader

**Nothing is stored.** No table, column, setting, file or sync field changes; schema stays at
version 3. Everything below lives in memory while the editor or the reading page is open (FR-015,
FR-033). The persisted `Note` (title, text, category, favorite, …) is exactly the one of feature 004.

## In-memory entities

### EditField

`Title`, `Text`, `Category`, `Favorite`. Values are UTF-16 strings (`std::u16string`): the field
content for `Title`/`Text`, the category name (empty = none) for `Category`, `"1"`/`""` for
`Favorite`. Offsets are UTF-16 code units, the unit Qt's `TextEdit`/`TextInput` use.

### EditStep

| Field | Meaning |
|-------|---------|
| `field` | which `EditField` changed |
| `pos` | first changed UTF-16 offset (`Title`/`Text`) |
| `removed` | text that was there before (may be empty) — or the old value (`Category`/`Favorite`) |
| `inserted` | text that is there now (may be empty) — or the new value |
| `first_ms`, `last_ms` | clock of the first and last merged observation (grouping) |
| `chars` | `removed.size() + inserted.size()` (memory cap) |

Rules:

- A step is never empty (`removed != inserted`).
- `Title`/`Text` steps are minimal: the common prefix and suffix are excluded (`pos` is after the
  common prefix, `removed`/`inserted` are the middles).
- A step may merge with the newest step when all hold: same field; both are plain typing
  (`removed` empty and `inserted` ≤ 80 characters after merging, containing no `\n`) or both are
  single-character backward/forward deletions in a run; positions adjacent; `now − last_ms ≤
  1500 ms`; no open group; the newest step is the one just pushed (the redo stack was empty).
- `Category` and `Favorite` steps never merge.
- A new step clears the redo stack (FR-014).

### EditHistory

| Part | Meaning |
|------|---------|
| `current[4]` | the last known value of each field |
| `undo`, `redo` | stacks of `EditStep` (newest on top) |
| `group` | none, or `{field, value_at_begin}` while a compound command runs |
| `applying` | set by `undo()`/`redo()` until `finish_apply`; `observe()` ignores calls |
| caps | `max_steps = 200`, `max_chars = 2,000,000`; the oldest steps are dropped, the newest is always kept |

State transitions:

```
(inactive) --reset(values)--> idle
idle --observe(f, v, ms)--> idle            # v == current[f]: nothing; else a step is pushed/merged
idle --begin_group(f)--> grouping --end_group()--> idle   # one step for the whole group
idle --undo()--> applying --finish_apply(values)--> idle  # step moved undo -> redo
idle --redo()--> applying --finish_apply(values)--> idle  # step moved redo -> undo
```

`can_undo()` ⇔ undo stack non-empty; `can_redo()` ⇔ redo stack non-empty. Both are false right
after `reset` (US2-5, US2-8). `reset` is the only way the opened content is "baselined"; there is
no step that removes it.

### EditApplied (the result of undo/redo)

| Field | Meaning |
|-------|---------|
| `field` | the field to change |
| `pos`, `remove_len`, `insert` | for `Title`/`Text`: delete `remove_len` UTF-16 units at `pos`, then insert `insert` there |
| `value` | for `Category`/`Favorite`: the value to set |
| `sel_begin`, `sel_end` | the range to select afterwards (equal = caret only) |

Undo of an insertion: remove it, caret at `pos`. Undo of a removal: re-insert, select the restored
text. Undo of a replacement: restore, select the restored text. Redo of an insertion or a
replacement: caret after the inserted text. Redo of a removal: caret at `pos`.

### Selection (editor)

The `selectionStart`/`selectionEnd` of the field that has focus (`activeField`). It exists only in
the QML item; the page also keeps `lastSelection {field, start, end}` (the last non-empty
selection) so *Extend* can use it after a tap has already collapsed the selection, and
`extendArmed` (bool). Rules: Copy/Cut/Del act only when `start < end`; typing replaces the
selection (native); Cut/Del/Paste-over run inside `begin_group`/`end_group` (one step).

### Clipboard text

One string. Sources: `ClipboardBridge.copy(text)` (Copy, Cut, reader Copy) and, when the platform
has a clipboard, whatever it holds. `hasText` ⇔ the cleaned text is non-empty. Cleaning
(`clean_pasted_text`): valid UTF-8, no control characters except `\n` and `\t`, `\r\n`/`\r`/U+2028/
U+2029 → `\n`, at most 1,000,000 characters. Title use applies `single_line`. Never written to
disk, logs, the database or the server.

### Source-offset model (parser, for the reader copy)

Added to `Span` and `Block` (`core/include/pbnotes/markdown.h`); defaults mean "unknown", so every
existing consumer and test behaves as before.

| Field | Meaning |
|-------|---------|
| `Span::src` | one entry per byte of `Span::text`: the byte index of that byte's origin in the **source line** of the block; for the character produced by a backslash escape (`\*`) the index of the backslash |
| `Span::el_begin`, `Span::el_end` | source extent `[begin, end)` in the line of the formatted element the span belongs to, marks included (`**bold**`, `[label](url)`, `<url>`, `![alt](url)`); for plain text the extent of its own characters; −1 = unknown |
| `Block::src_begin`, `Block::src_end` | source extent in the line of the whole block, list/heading/task prefix included; for an `Attachment` block the reference as written; a `Table` block uses the line indexes only |

Invariants (checked in `test_markdown_source_map`): `src.size() == text.size()`; `src` is strictly
increasing within a span *and across the spans of a block*, except that the bytes of one escape or
one multibyte character may repeat/advance by one; every `src[i]` lies inside
`[el_begin, el_end)` of its span; the block extent contains all of its spans' extents; the line
text sliced by a block's extent equals the block's source text.

`copy_source(note_text, blocks, from, to)` is a pure function over these (contracts/core-library.md).

### Reader selection (QML + `readerCopy`)

Units in order: `title` (only if `hasTitle`), then one unit per entry of `note.blocks`. A selection
is `{fromUnit, fromOffset, toUnit, toOffset}` with offsets in UTF-16 units of the unit's displayed
text (0 for a whole unit). The page normalizes it so `from ≤ to`. Whole units (table rows,
attachments, blank lines) have length 0/1 and are selected when they lie inside the range.
`readerCopy` turns it into `copy_source` arguments; the note is only read.

## Validation summary (where each rule is enforced)

| Rule | Enforced in |
|------|-------------|
| History grouping, caps, undo/redo order | `EditHistory` (core) |
| Pasted text cleaned; title single-line | `clean_pasted_text` / `single_line` (core), `ClipboardBridge.titlePaste` (bridge, 255-code-point check) |
| Title at Save: trim, ≤ 255, no control characters, non-empty | unchanged (`validate_title`, feature 004) |
| Copy from the reader returns source text | `copy_source` (core) |
| Reader never writes | `readerCopy` is `const`; test asserts the note row is unchanged |
