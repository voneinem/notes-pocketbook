# Contract: UI components and the one bridge addition

The interfaces this feature introduces are internal: QML components that every screen assembles (so the whole app
reads as one style, FR-005..FR-011), one bridge property, and one asset set. They are contracts because the QML tests
and the pages rely on them. The core library and the server API do not change (no `core-library.md`, no `api` file).

## 1. Bridge: `NotesBridge::listLayout`

```text
Q_PROPERTY(QString listLayout READ listLayout WRITE setListLayout NOTIFY listLayoutChanged)
```

- Reads the setting `list_layout`; returns `"list"` unless the stored value is `"tile"`.
- `setListLayout(v)` ignores a value that is not `"list"` or `"tile"` and a value equal to the current one; otherwise it
  stores the setting and emits `listLayoutChanged`. A failed write leaves the property unchanged and emits nothing.
- Same shape as `AttachmentsBridge::pickerView`. The QML test stub (`tests/qml/stubs.qml`) gains
  `property string listLayout: "list"` and a write counter, like `pickerViewWrites`.

No other bridge member changes. `bridge.view`, `bridge.sortOrder`, `bridge.searchText`, `bridge.fontSize`,
`bridge.fontPx`, `bridge.syncActive`, `bridge.syncStatus`, `bridge.pendingCount`, `device.quit()` are read as before.

## 2. `Style` (singleton, `qml/Style.qml`)

Read-only constants listed in `data-model.md`. Contract: **no page or component states a pixel size, line width or color
of the look literally**; it reads `Style`. `Style.topInset` is the single offset for the system status bar (R-9).

## 3. Components

Common rules for every control: it has a `text` string (its label, also the fallback when its icon cannot be loaded),
a `clicked()` signal, an `enabled` flag whose disabled look is grey (never colour), a pressed look of light grey, and a
hit area of at least `Style.touchMin` in both directions. Nothing animates.

| Component | Properties | Signals | Behavior |
|-----------|------------|---------|----------|
| `Icon` | `source` (icon name), `size`; `ready` | – | `Image` of `icons/<name>.svg` resolved relative to `Icon.qml` (the files of `qml/icons/`, listed in `qml.qrc`) at `sourceSize = size`; an empty or missing name draws nothing and `ready` is false, so the owning control shows its `text` |
| `IconButton` | `text`, `icon`, `toggled` (draws `<icon>-filled` when true: only for icons that have one), `iconSize`, `enabled` | `clicked()` | icon only, no border, hit area `Style.touchMin` square; the label is drawn only when the icon is not `ready` |
| `ToolButton` | `text`, `icon`, `toggled`, `enabled` | `clicked()` | icon over a one-line label (elided), 160 × 96 (its width is set by a `BottomPanel`) |
| `TopBar` | `title`, `leftItems`, `rightItems` (lists of items) | – | height `Style.barHeight + Style.topInset`; the title is `Text` `objectName: "topBarTitle"`, upper-cased by `font.capitalization`, centred, elided; hairline `objectName: "topBarRule"` beneath |
| `BottomPanel` | `tools` (list of items) | – | hairline `objectName: "bottomPanelRule"` above; the visible tools share the width in equal cells (re-laid out when one is shown or hidden) |
| `DialogBox` | `title`, `footer` (`[{text, role}]`, role `dismiss` or `accept`), default content slot (`contents`) | `closeRequested()`, `footerClicked(role)` | the box: 4 px outline, radius 18 (a `Rectangle`, `objectName: "dialogBox"`), bold title + `Close` `IconButton` over a rule (`"dialogRule"`), the content, a footer of equal cells of `FooterButton` split by `"footerRule"`s, dismiss first; the owner sets `width` and places it |
| `DialogFrame` | as `DialogBox` (`title`, `footer`, content), `boxY` (centred when negative), `boxWidth`; `open()`, `close()` | `closeRequested()`, `footerClicked(role)` | a `DialogBox` over a full-window white scrim at `Style.scrim` that swallows touches; `z: 1000`, hidden until `open()` |
| `FooterButton` | `text`, `enabled`, `font` | `clicked()` | `objectName: "footerButton"`; borderless, upper-case, 120 px high, fills its cell |
| `PopupBubble` | `pointerX` (in its own coordinates), `pointerUp` (pointer on the top edge when true), `showPointer`, content slot | – | 4 px outline, radius 18, `bubble-pointer` image (`objectName: "bubblePointer"`) at `pointerX` clamped to the rounded corners; the owner sets `width`/`height` and places it |
| `InkField` | `label`, `text`, `placeholderText`, `errorText`, `field` (the `TextField`), `forceFocus()` | `accepted()`, `textEdited()` | `objectName: "inkField"`; label above, 2 px outline of radius 10, error line (`✗ …`, bold) under it |
| `InkSwitch` | `checked`, `text`, `enabled` | `toggled(on)` | a 170 × 76 pill; the chosen half is filled black with `on` or `off` in white; the owner sets `checked` |
| `InkSlider` | `stops`, `index`, `label` | `moved(index)` | `Decrease`/`Increase` round `IconButton`s (off at the ends), `objectName: "sliderTrack"` with end dots and the ring knob `"sliderKnob"`; a tap on the track reports the nearest stop; only reports, the owner sets `index` |
| `InkRow` | `text`, `value`, `chevron`, `enabled` | `clicked()` | a settings row 110 px high: label left, value and chevron right, `"rowDivider"` underneath |
| `RowDivider` | – | – | a 2 px tall dotted hairline (tiled `row-dots` image), `opacity` 0.55 |

### Existing components keeping their interface (only their look changes)

| Component | Kept interface | Look after the change |
|-----------|----------------|-----------------------|
| `ConfirmDialog` | `title`, `text`, `acceptText`, `cancelText`, `showCancel`, `open()`, `close()`, `accepted()`, `rejected()` | a `DialogFrame`; footer `CANCEL | OK` (dismiss first); the close icon rejects |
| `TextEntryDialog` | `title`, `placeholder`, `errorText`, `acceptText`, `cancelText`, `open(initial)`, `close()`, `accepted(text)`, `rejected()` | a `DialogFrame` with an `InkField`, `boxY` 120 so the keyboard does not cover it; `accepted` leaves it open as before |
| `ChoiceDialog` | `title`, `choices`, `current`, `open()`, `close()`, `chosen(value)` + new `anchor` (an `Item`) | a `PopupBubble` (700 px wide at most) under `anchor` with its pointer on the anchor's centre, above it when there is no room below, centred without a pointer when none; rows 96 px with a `check` for the current one and `"rowDivider"`s; paged by `PageButtons` inside when long; a tap outside closes; `title` is the accessible name (not drawn) |
| `SelectionMenu` | `items`, `anchorX`, `selTop`, `selBottom`, `minY`, `maxY`, `open()`, `close()`, `chosen(id)` | a `PopupBubble` of icon-only `IconButton`s (`cut`, `copy`, `paste`, `remove`→`trash`, `all`→`select-all`, `undo`, `redo`; an id without a picture draws its label); the pointer is on the bottom edge when the bubble sits above the selection |
| `SlashMenu` | `entries`, `anchorX`, `caretTop`, `caretBottom`, `minY`, `maxY`, `open()`, `close()`, `chosen(id)` | a `PopupBubble` of cells in **two columns**, row by row, each an icon `slash-<id>` and the label, with dotted dividers under the cells of all but the last row (two columns keep the twelve entries on one screen without scrolling) |
| `DateTimePicker`, `EmojiPicker` | unchanged (`open(date)` / `open()`, `minY`, `maxY`, `accepted(text)` / `chosen(emoji)`, `cancelled()`) | a `DialogBox` placed between `minY` and `maxY` as before (not a full-window modal); title with close icon (cancels), footer `CANCEL | INSERT` / `CANCEL` |
| `PageButtons` | `target`, `snap`, `unsplittable`, `showIndicator` | two chevron `IconButton`s `Page up`/`Page down`; with `showIndicator` the text `n / total` (`objectName: "pageIndicator"`) sits between them (`pageNumber`, `pageCount` are estimates from the distance one page moves) |

Because the interfaces are kept, the driver scripts under `tests/qml/` keep addressing controls by `text` and
`objectName`; where a script clicked a text button that becomes an icon, the icon control has the same `text`.

## 4. Per-screen contract (what each screen must contain)

Derived from the spec's requirements; this is the checklist the structure tests and the device walkthrough use.

| Screen | Must contain | Must no longer contain |
|--------|--------------|------------------------|
| Note list | `TopBar` (home, menu, `NOTES`, search, new); search row (opens from the search icon); filter/sort row (view label ↓ and sort label ↓, each opening a bubble; layout toggle); count line (`N NOTES`) with the sync state at its right; rows (bold title, category/date line, favorite star, pending marker) **or** cards; page chevrons | *New*, *Sync now*, *Settings*, *Quit*, *View:*, *Sort:*, *Clear* as bordered buttons |
| Reader | `TopBar` (back, title, edit); a caption line (favorite star, category) as the first line of the scrolling text; a bottom bar with the *Text size* tool and `PageButtons` with `n / total`; the text-size control (`InkSlider` with 3 stops, label `N pt`) in a panel above the bottom bar that the tool opens | *Back*, *A-*, *A+*, *Edit* as bordered buttons |
| Editor | `TopBar` (back, title, undo, redo, favorite, save); the title as a labelled field (the page's own `TextField`, outlined like an `InkField`); text area; `BottomPanel` above the keyboard with *Attach*, *Category* (showing the chosen category), *Delete* (only for a saved note) | *Cancel*, *Save*, *Favorite*, *Attach*, *Delete*, the category button |
| Dialogs / pop-ups | as in §3 | bordered text buttons, the old square-cornered 4 px box |
| Settings, About, licenses, setup | `TopBar` (back, upper-case title); hairline rows; `InkSwitch` for Auto Sync; `InkField` in setup | *Back* and every action as a bordered button |
| Attach browser | `TopBar` (back, `ATTACH`, layout toggle); path line; *Up* as an up-chevron in the bar; the rest as feature 009 | *Cancel* and *Up* as bordered buttons |

## 5. Asset set (`qml/icons/*.svg`)

Fifty files, one per name, drawn on a 96 × 96 grid with a 4 px stroke and round caps (the on-screen size is set by the control,
56–80 px): `attach`, `bubble-pointer` (a 40 × 24 triangle, the one exception to the grid), `check`, `chevron-down`,
`chevron-left`, `chevron-right`, `chevron-up`, `close`, `copy`, `cut`, `download`, `eye`, `eye-filled`, `filter`, `folder`,
`gear`, `grid`, `home`, `info`, `list`, `menu`, `minus-circle`, `note-plus`, `paste`, `pencil`, `plus-circle`, `redo`,
`row-dots` (a 6 × 2 tile), `save`, `search`, `select-all`, `star`, `star-filled`, `sync`, `tag`, `text-size`, `trash`, `undo`,
and the twelve menu icons `slash-bold`, `slash-bullet_list`, `slash-code_block`, `slash-details`, `slash-emoji`,
`slash-info_box`, `slash-italic`, `slash-link`, `slash-quote`, `slash-strikethrough`, `slash-table`, `slash-underline`.
Each is a single group of paths, black stroke or fill, transparent background, no text, no external references.
`tests/qml/ui_style.qml` loads each through `Icon` and fails on an error status; a file missing from `qml.qrc` shows up in
`app_self_test` and on the device as a control that falls back to its label.

## 6. The launcher icon

`res/icons/pbnotes.bmp` / `pbnotes_f.bmp`: 122 × 122, 8 bpp, opaque, generated by `scripts/make-icons.sh`. Contract
(tested): the outer 10 px ring of the canvas is entirely the variant's background value (white in the normal variant, black in
the focused one), so there is no frame, the glyph's bounding box is 60–85 % of the canvas in both directions and centred within
4 px, and the two variants are different pictures.
