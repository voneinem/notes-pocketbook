# Contract: QML bridge and controls

Extends `specs/005-text-editing-reader-title/contracts/qml-bridge.md`. The one new context property
is set in `app/main.cpp` next to `editSession` and `clipboard`.

## Reader blocks (`reader_blocks`, changed)

Each entry of `openNote(...).blocks` keeps its fields (`kind`, `block`, `row`, `src`, `line`,
`html`, `links`, `level`, `ordered`, `number`, `depth`, `task`, `checked`, `unsplittable`). Added:

| Field | Type | Meaning |
|-------|------|---------|
| `kind` | string | new values `"fence"`, `"code"`, `"callout"`, `"summary"`, `"marker"` |
| `quote` | int | 0 none, 1 quote, 2 info box — on blocks inside a quote group (any kind) |
| `quoteFirst`, `quoteLast` | bool | first / last block of the group |
| `codeFirst`, `codeLast` | bool | first / last block of the code box (`fence` and `code` blocks) |
| `callout` | string | `"note"`, `"tip"`, `"warning"`, `"important"`, `"caution"` (on `callout` blocks) |
| `detailsId` | int | −1, or the key of the section the block belongs to (`summary` and content blocks) |
| `detailsOpen` | bool | initial open state (`<details open>`) |
| `unsplittable` | bool | now also true for `code` and `fence` blocks (a line is never cut by a page break) |

`html` is the escaped markup of the block's spans: `<b>`, `<i>`, `<s>`, `<u>`, `<a href="lN"><u>…
</u></a>`; a `code` block's html is its line, escaped, with no formatting. `marker` and `fence` blocks
have empty html. The same function builds table-cell html (so strike and underline show in cells).

## `slash` (`SlashBridge`, new, in `bridge/include/edit_bridge.h`)

Thin wrapper over `pbnotes/slash.h`; converts `QString` ↔ UTF-16. Stateless.

| Member | Behavior |
|--------|----------|
| `entries()` | `[{id, label}]` — the twelve entries in menu order; labels are `tr()` strings |
| `detect(before, after, caret, menuPos)` | `{trigger: "none"\|"menu"\|"datetime", pos}` (`menuPos` −1 = no menu open) |
| `expand(id, text, pos, len)` | `{begin, end, text, caret}` for an entry id other than `"emoji"`; `{}` for an unknown id |
| `replace(pos, len, insert)` | `{begin, end, text, caret}` — plain replacement (emoji, date) |
| `timestamp(year, month, day, hour, minute)` | `"YYYY-MM-DD HH:MM"` |
| `daysInMonth(year, month)` | 28..31 |
| `emojis()` | the 48 emojis of the grid, each one code point |

No signal, log line or exception carries note text. `entries()` and `emojis()` are constants.

## Editor flow (`EditorPage.qml`, changed)

State: `previousText`, `programmatic`, `slashState` (`{pos, kind}` or null; named so as not to hide the context property `slash`) — see data-model.

1. `area.onTextChanged`: if `programmatic` → no detection. Otherwise
   `d = slash.detect(previousText, text, cursorPosition, slashState ? slashState.pos : -1)`.
   - `d.trigger === "menu"`: `editSession.beginGroup("text")`, then `observe("text", text)` as
     always, open `SlashMenu` below the caret, `slashState = {pos: d.pos, kind: "menu"}`.
   - `d.trigger === "datetime"`: close the menu, open `DateTimePicker`, `slashState.kind = "datetime"`
     (the group stays open).
   - `"none"` while a popup is open: `closeSlash()` first (`editSession.endGroup()`), then `observe`.
   Finally `previousText = text`.
2. An entry is chosen: `e = slash.expand(id, text, slashState.pos, 1)` (`"emoji"` opens `EmojiPicker`
   instead; the grid then calls `slash.replace(slashState.pos, 1, emoji)`). With `programmatic = true`:
   `area.remove(e.begin, e.end)`, `area.insert(e.begin, e.text)`, `area.cursorPosition = e.caret`;
   then `editSession.observe("text", area.text)` and `editSession.endGroup()` → one step; the popup
   closes; the area keeps focus and the keyboard stays.
3. The picker's Insert uses `slash.replace(slashState.pos, 2, slash.timestamp(...))` the same way;
   Cancel / Back call `closeSlash()` and leave the typed `//`.
4. `closeSlash()` — `editSession.endGroup()`, hide the popup, `slashState = null` — is called by: any
   non-trigger text change, a caret move (`onCursorPositionChanged` not caused by us), loss of focus,
   a tap outside the popup (the scrim `MouseArea` behind it), a rotation (`onWidth/HeightChanged`),
   any menu or header command (long-press menu, Attach, Save, Cancel, Favorite, Category), Back.
5. The title field never calls `detect`; `Keys.onReturnPressed` etc. unchanged.
6. `function backHandled()` — returns true (after `closeSlash()`) when a popup is open, else false.

## Back (`Main.qml`, changed)

```
onBackRequested: {
    var p = nav.currentItem
    if (p && p.backHandled && p.backHandled()) return
    ... existing: pop when nav.depth > 1
}
```

## QML components (new)

| Component | Contract |
|-----------|----------|
| `SlashMenu` | `property var entries`, `property real anchorX, caretTop, caretBottom, minY, maxY`; `signal chosen(string id)`; a bordered bubble (same look as `SelectionMenu`) with a 2-column `Grid` of `InkButton`s (`focusPolicy: Qt.NoFocus`) in `entries` order; `place()` puts it below the caret line, above it when there is no room above `maxY`; scrolls (Flickable) only if taller than the room. Never takes focus; no animation |
| `DateTimePicker` | `property int year, month, day, hour, minute` (preset by the page from the current local time); five `▲ value ▼` columns of `InkButton`s; day range from `slash.daysInMonth`, clamped when month or year changes; `signal accepted(string text)` (text from `slash.timestamp`), `signal cancelled()`; Insert and Cancel buttons; takes no focus |
| `EmojiPicker` | `property var emojis`; a 6×8 grid of buttons, each at least 78×78 px, `signal chosen(string emoji)`, `signal cancelled()`; takes no focus |

All three are children of the editor page (`z` above the text), laid out for the current width, and
close on rotation.

## Reading page (`ReaderPage.qml`, `BlockText.qml`, changed)

- `BlockText` draws, from the block fields: a code box (1 px-equivalent 3 px border, light fill
  `#f2f2f2`, fixed-width font at 0.9 × body size, `codeFirst` draws the top edge, `codeLast` the
  bottom, every block the sides); `fence` blocks are a 14 px strip carrying the box edge; a
  `quote == 1` block has a 6 px left bar and indent; `quote == 2` draws a 3 px border box around its
  group; `callout` shows its kind as bold text ("Note" …) in the box; `summary` shows `▶` / `▼` and
  its text, tap toggles; `marker` has height 0.
- Heading sizes: scale `[1.9, 1.55, 1.3, 1.15, 1.1, 1.05]` × body size, bold.
- `ReaderPage`: `property var openDetails: ({})`; `detailsOpen(b)` = `openDetails[b.detailsId]` if
  set else `b.detailsOpen`; content blocks of a closed section are `visible: false`; tapping a
  `summary` toggles its entry (a tap while a selection exists only ends the selection, as for links);
  `unitIsText` is false for `fence`, `callout`, `marker`; `unitAtY` and the markers skip invisible
  units; `refreshUnsplittable` also lists every heading's rectangle extended by one body line
  (keep with next, research R-6) and the `code`/`fence` blocks.
- Copy goes through the unchanged `bridge.readerCopy(...)`; hidden units inside the range are copied.
