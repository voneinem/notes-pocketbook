# Contract: QML bridge and controls

Extends `specs/001-pocketbook-notes-mvp/contracts/qml-bridge.md` and
`specs/004-title-category-picker/contracts/qml-bridge.md`. New context properties are set in
`app/main.cpp` next to `bridge`, `device`, `attachments`, `about`.

## `editSession` (`EditSession`, new, `bridge/include/edit_bridge.h`)

One object; the editor calls `reset` when it opens. Wraps `pbnotes::EditHistory`, converting
`QString` ↔ UTF-16 and supplying the clock (`QElapsedTimer`; a `setClockForTest(fn)` seam).

| Member | Behavior |
|--------|----------|
| `reset(title, text, category, favorite)` | starts the history at the opened values |
| `observe(field, value)` | `field` is `"title"`, `"text"`, `"category"` or `"favorite"` (`favorite` value `"1"`/`""`); reports a new value; ignored while applying |
| `beginGroup(field)` / `endGroup()` | one step for a compound command |
| `canUndo`, `canRedo` (properties, `changed` notify) | enabled state of the Undo/Redo buttons |
| `undo()` / `redo()` | `{field, pos, removeLen, insert, value, selBegin, selEnd}` to apply (empty map = nothing to do) |
| `finishApply(title, text)` | ends the applying state; the editor passes the field values it now holds |


## `clipboard` (`ClipboardBridge`, new, same header)

| Member | Behavior |
|--------|----------|
| `hasText` (property, notify) | the cleaned clipboard text is non-empty; updated on `QClipboard::dataChanged` and on `copy` |
| `copy(text)` | cleaned (`U+2029 → \n`, control characters dropped), then stored in the platform clipboard when there is one, otherwise in the in-app buffer; an empty result is ignored |
| `text()` | the current text (platform or in-app), *not yet cleaned*; callers go through `cleanPaste` / `titlePaste` |
| `cleanPaste()` | `{ok, text, errorKey, errorText}`: `clean_pasted_text` of `text()`, for the note text |
| `titlePaste(current, selBegin, selEnd)` | `{ok, text, caret, errorKey, errorText}`: the title after replacing the selection (UTF-16 offsets) with `single_line(clean_pasted_text(text()))`, caret after it; `ok:false` with `title_too_long` ("The title is too long (at most 255 characters).") when the result exceeds 255 code points, or the clean error |

Error texts added to `NotesBridge::message(key)`: `paste_invalid` ("The copied text cannot be
pasted."), `paste_too_large` ("The copied text is too large to paste.").

No signal, log line or exception carries text. When no `QGuiApplication` exists (host tests) only the
in-app buffer is used.

## `NotesBridge` (changed)

| Member | Change | Behavior |
|--------|--------|----------|
| `openNote(localId)` | + `hasTitle`, `titleText` | `titleText` is the stored title as is; `hasTitle` is true when it is non-empty after trimming; `title` stays `display_title(...)` for existing users (list) |
| `openNote(localId).blocks[]` | + `block`, `row`, `src` | `block`: index in the parse result; `row`: table row (0 for non-table); `src`: `{begin, end}` of the block in its line (informational) |
| `readerCopy(localId, fromUnit, fromOffset, toUnit, toOffset)` | new, `const` | units as in data-model (the title is unit 0 when the note `hasTitle`, which the bridge knows itself); offsets are UTF-16 units within the unit; returns the source text to copy; never writes. An invalid range returns `""` |

## QML components

> **Superseded (2026-10-11):** `EditTools.qml` and the footer buttons are replaced by `SelectionMenu.qml` (a bubble with
> text buttons: `items` [{id, label}], `chosen(id)`), `SelectionHandle.qml` (one marker) and `SelectionHandles.qml` (the
> two markers of a text field). The editor's menu items are Cut, Copy, Paste, Remove, Select all, Undo, Redo (only those
> that apply); the reading page's are Copy and Select all. Gestures are `MouseArea` `pressAndHold` / `clicked`.

### `EditTools.qml` (superseded, see above)

A `Row` of buttons, all `focusPolicy: Qt.NoFocus` (FR-010). Properties from the page: `canCopy`
(selection in the active field), `canPaste`, `canUndo`, `canRedo`, `extendArmed`. Signals:
`undo()`, `redo()`, `selectAll()`, `extend()`, `copy()`, `cut()`, `paste()`, `remove()`. Labels:
Undo, Redo, All, Extend, Copy, Cut, Paste, Del; disabled ones use `InkButton`'s greyed look.
Extend shows as pressed (`★ Extend`) while armed.

### `EditorPage.qml` (changed)

- Order: button row, **tools row**, Title, Category, notes, rule, text.
- `activeField` follows `titleField.activeFocus` / `area.activeFocus`; the commands act on it.
- Both fields call `editSession.observe(...)` from their text-change handlers, the category picker
  and the Favorite button likewise. `Component.onCompleted` sets the texts first, then
  `editSession.reset(...)` (the setting does not record).
- Copy: `clipboard.copy(f.selectedText)`. Cut/Del: `beginGroup`, `f.remove(selStart, selEnd)`,
  `endGroup`; Cut copies first. Paste: text from `clipboard.text()` through `cleanPaste` (text) or
  `titlePaste` (title); in a group, replace the selection or insert at the caret. A refusal puts
  the error in the page's message line and leaves the field unchanged.
- Undo/Redo: call `editSession.undo()/redo()`, apply to the field (`remove` then `insert`, then
  `select`/`cursorPosition`) or set `category`/`favorite`, then `finishApply(titleField.text,
  area.text)`.
- Press-and-hold (a `TapHandler` per field) calls `selectWord()`. Extend arms `extendArmed`; the
  next tap in the same field selects from the remembered word to the tapped position; any other
  button disarms.
- `dirty` is unchanged (a comparison with the opened values), so a full undo clears it (FR-017).
- Attachments: `insertToken` is unchanged; its single `insert` is one step (FR-016).

### Reader (`ReaderPage.qml`, `BlockText.qml`, `TableRowView.qml`, `AttachmentView.qml`, `TitleBlock.qml`)

- `TitleBlock` is the first item of `body` when `note.hasTitle`: bold, `fontPx × 1.7`, wrapped,
  24 px below, selectable like a text block; absent otherwise (FR-024).
- `BlockText` shows its `html` in a read-only `TextEdit` (`RichText`, inverted selection colors,
  no focus on press). Properties from the page: `selFrom`, `selTo` (UTF-16 offsets, −1 = none);
  signals: `wordHeld(start, end)` (long press), `tapped(pos)` (a tap that is not on a link),
  `linkActivated(link)`, `taskToggled(line)` as today.
- `TableRowView`, `AttachmentView`, blank lines: `selected` (bool) draws a 4 px black frame.
- The page keeps the selection `{fromUnit, fromOffset, toUnit, toOffset}` and `anchor`, normalizes
  it, hands each unit its range, and clears it on: a tap outside the selection (that is neither
  Extend-armed nor a link/task/attachment action), `load()`, a font-size change, leaving the page.
  Page turns and the edge taps never clear it.
- Footer row: *All*, *Extend*, *Copy* beside `PageButtons`. Copy calls
  `clipboard.copy(bridge.readerCopy(...))`; a note's title, text and modified time are never
  changed by any of them.
