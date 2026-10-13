# Research: Built-in Look (GUI Overhaul)

Every unknown of the Technical Context is resolved below. Items marked **[DEVICE]** cannot be decided on the host;
each has a decision that works either way plus a check in `quickstart.md`.

Ground truth used: the ten reference screenshots in `screens/` on the reader (see the table in `spec.md`), the
current QML (`qml/`, `qml/pages/`), the packaging script and the app log of the 1.5.0 device run
(`image.formats list=…,png,…,svg,svgz,…`).

## R-1. How the line-art icons are drawn

- **Decision**: one monochrome **SVG file per icon** under `qml/icons/` (next to the QML, so the same relative path works from the file system in host tests and from the resource bundle on the reader), listed in `qml/qml.qrc` and shown by one
  small component `Icon.qml` (`Image` with `sourceSize` set to the on-screen size, `smooth: true`, no color
  overlay). Filled variants (star on, selected tab) are separate files. The icons are drawn for this project (own work,
  MIT like the rest); the device's built-in icon files are not copied.
- **Rationale**: the device log of 1.5.0 lists `svg` among the supported image formats, so the Qt SVG plugin is on
  the reader; SVG stays crisp at any size and in any orientation, needs no per-size assets, and a path with a 3 px
  stroke is a few hundred bytes. The existing code draws glyphs from rectangles (the attach browser's layout toggle,
  spec 009), which does not scale to curves such as a star, a gear or a magnifier.
- **Safety net**: `Icon.qml` shows the control's `text` (its label) in the same place when the image reports an error,
  so a missing SVG plugin degrades to labelled text instead of blank buttons. A unit-level QML test loads every icon
  file and fails on any error status (catches a typo in a file name on the host).
- **Alternatives considered**: *PNG rendered from SVG at build time* — works without a plugin but needs a rasterizer
  in the build and a second asset set for every size; rejected while the plugin is confirmed present. *An icon font*
  (one TTF, glyph per icon) — compact, but needs a font-building tool chain and a licence entry for a generated file.
  *Qt Quick Shapes / Canvas* — path code in QML for 25 icons, slower to draw on e-ink and not available as a
  confirmed module on the reader. *Unicode symbols in the text font* — the current stop-gap (`★`, `✓`); weights and
  shapes depend on the font and do not match the device's icon set.

## R-2. Where the shared look lives

- **Decision**: a QML singleton `Style` (`qml/Style.qml`, with `qml/qmldir`) holds every constant the screens share:
  the ink colors (`#000000`, hairline grey, pressed grey, disabled grey), line widths (hairline 1, rule 2, heavy 4),
  radii, type sizes (title, body, caption, label), the top bar and row heights, the touch-target minimum and the
  `topInset` (R-9). Pages and components read `Style.x` and never hard-code a pixel value of the look.
- **Rationale**: FR-005..FR-011 require one consistent look; with 17 QML files today each repeating `#111111`,
  `border.width: 2` and `font.pixelSize: 38`, a constant set changed in one place is the only way to tune the look
  against the screenshots on the device without touching every file. The pages already `import ".."` to reach
  `InkButton`, so a `qmldir` in `qml/` is picked up by the same import.
- **Risk**: a singleton needs the `qmldir` file in the resource list and, on the ARM build (manual `rcc`,
  `cmake/qt_manual.cmake`), the same path layout. A first test loads a page that reads `Style.ruleWidth` on the host,
  and the app `--self-test` (every page instantiates) covers the device build; both come before any restyling.
- **Alternatives considered**: *properties on each component with copies of the numbers* — drifts. *A context
  property from C++ like `bridge`* — puts presentation constants in the bridge, which the stub-based QML tests would
  then have to fake. *`qt_add_qml_module`* — would change the manual ARM build for one file.

## R-3. Which shared components

- **Decision**: add the smallest set that every story needs, each in its own file, and restyle the existing ones in
  place so call sites change as little as possible:

  | Component | Replaces / used by | Notes |
  |---|---|---|
  | `Icon.qml` | every icon | R-1 |
  | `IconButton.qml` | bordered `InkButton` for icon-only actions | has `text` (label for tests and fallback), `icon`, `toggled` (filled variant), `enabled`; hit area ≥ today's button height |
  | `ToolButton.qml` (icon over label) | bottom-panel tools | same `text`/`icon`/`toggled` contract |
  | `TopBar.qml` | the header rows of every page | left slot, centred upper-case title, right slot, hairline beneath, `Style.topInset` |
  | `BottomPanel.qml` | reader and editor tool rows | hairline above, evenly spaced `ToolButton`s |
  | `DialogFrame.qml` | `ConfirmDialog`, `TextEntryDialog`, `DateTimePicker`, `EmojiPicker` | scrim, heavy rounded outline, bold title + close icon over a rule, footer of full-width upper-case buttons split by a rule |
  | `PopupBubble.qml` | `SelectionMenu`, `SlashMenu`, `ChoiceDialog` | rounded heavy outline plus a pointer (R-6) |
  | `InkSwitch.qml`, `InkSlider.qml`, `RoundButton` (inside `InkSlider`) | Settings on/off, the reader's text-size control | R-7 |
  | `InkField.qml` | the `TextField`s with ad-hoc `border.width: 2` | label above, thin rounded outline |
  | `FooterButton.qml` | the buttons of `DialogFrame` footers and the one wide action button a screen may have (`ALLE AUFKLAPPEN`) | no border, upper-case label, fills its cell; has `text` and `clicked` |
  | `RowDivider.qml` | list rows, choice rows, settings rows | dotted hairline |

  `InkButton` (the bordered text button) is **kept untouched until slice 5 and then deleted**: every use is replaced
  by one of the components above, so "no bordered text button anywhere" (FR-006) holds by construction. All the new
  controls keep a `text` property and a `clicked` signal, so the existing driver scripts that find buttons by
  `text` keep working.
- **Rationale**: the spec's sixteen screen elements collapse to these ten pieces; building per page would repeat the
  heavy outline and the footer in five dialogs. Everything stays QML, no C++.
- **Alternatives considered**: *restyling every page by hand* — the same constants and shapes repeated 17 times;
  *Qt Quick Controls styles (a custom `QQuickStyle`)* — a style plugin and a C++/QML-module build change for what is
  a dozen controls.

## R-4. Top-bar content per screen and what happens to today's buttons

- **Decision** (from the screenshots and the clarified FR-006/FR-017):

  | Screen | Top bar | Bottom / other |
  |---|---|---|
  | Note list | left: home (= leave the app, replaces *Quit*) and menu (opens a popup: *Sync now*, *Settings*); centre: `NOTES`; right: search, new note | filter/sort row, count line, sync status line, page chevrons |
  | Reader | left: back; centre: note title; right: edit (pencil) | caption line "★ category"; bottom panel: *Text size* tool + page indicator + page chevrons |
  | Editor | left: back (= Cancel, asks if dirty); centre: note title or `NEW NOTE`; right: undo, redo, favorite star, save | title as a labelled field under the bar; bottom panel above the keyboard: *Attach*, *Category*, *Delete* |
  | Settings, About, licenses, setup, attach browser | left: back; centre: upper-case screen title; right: nothing (browser: layout toggle) | rows / fields; the attach browser keeps its path line, *Up* becomes an up-chevron in the bar |

- **Reconciliation with the spec** (two small corrections, recorded in `spec.md`): (1) FR-017 listed *title* among the
  bottom-panel tools. The title is an input, not an action; it stays a labelled field directly under the top bar and
  the bottom panel carries *Attach*, *Category*, *Delete*. (2) The spec's "labels follow the device language" is
  wrong: the app has no translations and shows English only; this feature adds none, and the compact controls are
  sized for labels up to 30 % longer than today's so a later translation fits.
- **Why the editor's panel sits above the keyboard**: FR-023 forbids more taps than before. Today *Attach* is one tap
  even while typing; a panel hidden by the keyboard would cost a tap to dismiss it. The page already tracks
  `keyboardHeight`; the panel anchors to it. Its cost (about 140 px of text area) is offset by *Category* leaving the
  header (the header loses one 78 px row and the button row).
- **Alternatives considered**: *all editor actions in one overflow popup (the built-in editor's `ooo`)* — what the
  reference does, but the clarified answer chose a bottom panel and FR-023 favors visible actions; *keeping *Cancel*
  as a text link* — a bordered/boxed text control is exactly what FR-006 removes, and back already means cancel.

## R-5. The UI typeface **[DEVICE]**

- **Decision**: ask for family `Roboto` at start-up when the font database has it, otherwise keep the current default
  font (today's behavior), and log one line `font.ui family=<name> roboto=<0|1>` (no personal data) next to the
  existing `image.formats` line. **Gate before slice 2 closes**: read that line from the first device run. If Roboto is
  absent, bundle Roboto Regular, Medium and Bold (Apache License 2.0) beside the emoji font with a NOTICE section, a
  `licenses/manifest.txt` entry and a packaging line — the same pattern spec 002 used for Noto Emoji.
- **Rationale**: the built-in screens are Roboto (visible in every screenshot); matching typography is part of FR-010.
  Probing is cheaper than bundling ~500 KB and a licence entry that may not be needed, and the fallback keeps the app
  working either way. The sizes (`Style`) are tuned to the screenshots in device pixels: title ≈ 44 px upper-case,
  row title 40 px bold, caption 28 px, footer button 40 px.
- **Alternatives considered**: *always bundle Roboto* — certain, but adds size and a licence entry that a probe may show
  to be unnecessary; *keep the default font* — leaves FR-010 unmet if the default is not a Roboto-like sans.

## R-6. Pop-up bubble with a pointer

- **Decision**: `PopupBubble.qml` is a white `Rectangle` with a 4 px black border and radius 18, plus a **pointer** drawn
  from one SVG asset (`bubble-pointer.svg`, a triangle open at its base) placed on the top or bottom edge at
  `anchorX`, with a white 6 px patch that hides the border segment under it. `SelectionMenu` and `SlashMenu` keep their
  placement code (`place()`, `anchorX`, `minY`, `maxY`) and only swap their content: the selection menu becomes a row of
  icon-only entries (copy, select all, paste, …, as in screenshot 0007) with its label as the entry's `text`; the "/"
  menu becomes a single column of icon + label rows with dotted dividers (screenshot 0004). `ChoiceDialog` becomes a
  bubble anchored to the control that opened it (view and sort in the list, category in the editor), paged inside the
  bubble with up/down chevrons when it has more rows than fit.
- **Rationale**: the placement logic is the part with edge cases (keyboard, rotation, above/below); it already has tests
  and must not change. A pointer made of an asset avoids a `Canvas` or `Shape`.
- **Alternatives considered**: *`Canvas` pointer* — repaints on e-ink and adds an item that the offscreen tests render
  differently; *no pointer* — fails FR-015 ("a pointer toward their origin").

## R-7. Switch, slider and the reader's text size

- **Decision**: `InkSwitch` is a rounded pill with a 3 px outline, the filled half carrying the word `on`/`off`
  (screenshot 0009), toggled by a tap on the whole pill. `InkSlider` is a hairline track with end dots, a **ring knob**
  (black ring, white centre) and round minus/plus buttons at its ends; it has *stops*, not a continuum, because the
  reader's text size has three steps (`small` 30 px, `medium` 38 px, `large` 50 px; the setting `font_size` is
  unchanged). The label next to the slider shows the current size with its unit, computed from the pixel size at the
  panel's 300 ppi (`round(px × 72 / 300)`: 7, 9, 12 **pt**), and tapping the track moves the knob to the nearest stop.
- **Rationale**: FR-018 asks for the device's control look, not for new sizes; keeping three stops keeps the setting, the
  settings table and the editor (which shares `fontPx`) untouched. [DEVICE] If the computed points read wrong against
  the built-in "11 pt" at the same visual size, only the label formula in `Style` changes.
- **Alternatives considered**: *a continuous size 8–24 pt* — a new setting range, a reflow test matrix and a page-break
  test update for no requirement; *a text "A− A+"* — the current control, which FR-018 replaces.

## R-8. The note list's Tile layout

- **Decision**: a second view in `NoteListPage` next to the existing `ListView`, a `GridView` of cards (title bold,
  category/date line, favorite marker, border 2 px), 2 columns in portrait and 3 in landscape (`columns = max(2,
  floor(width / 420))`), only one of the two views active, both fed by the same `rows`. The paging buttons are reused
  with `snap` = the active view's cell height, the same technique the attach browser (spec 009) uses. The choice is a new
  setting `list_layout` (`list`|`tile`, default `list`), exposed as `NotesBridge::listLayout` exactly like `pickerView`
  on `AttachmentsBridge`. No thumbnails: notes are text.
- **Rationale**: copies a proven pattern from 009 (toggle, remembered setting, snap paging) so risk and test shape are
  known; the only new thing is the card.
- **Alternatives considered**: *reuse `picker_view` for both lists* — two unrelated screens sharing one preference would
  surprise the reader; *a card with a text excerpt* — the excerpt has to be produced by the core, a behavior change the
  spec excludes ("tiles show text only: title, category, date, favorite").

## R-9. The device's status bar and the top inset **[DEVICE]**

- **Decision**: `Style.topInset` (default 0) is the only place that offsets the top bars. The first device run of slice 2
  takes one screenshot of the note list (`scr…bmp` into `screens/`); if the system status bar overlays the app, the
  inset is set to its height (the built-in screens show it at ≈ 135 px including its hairline) in one line of `Style.qml`.
- **Rationale**: the reference screenshots show the clock/Wi-Fi/battery strip above every built-in top bar; whether the
  system draws it over this app's fullscreen window is not known from the repository (`Window.FullScreen`, see
  `Main.qml`). Both outcomes are handled by one constant.
- **Alternatives considered**: *draw our own clock/battery strip* — needs device state APIs not in the bridge and not
  required by the spec.

## R-10. The launcher icon

- **Decision**: `scripts/make-icons.sh` is changed to draw the glyph **without the outer frame**, supersampled 4× then
  box-filtered to 8-bit grey so the strokes are anti-aliased like the built-in icons; sheet outline 3 px, a header strip
  8 px instead of 16, three text lines, glyph about 76 × 92 px centred in the 122 × 122 canvas (the built-in icons
  occupy ≈ 75–90 px of their cell in screenshot 0001). The focused variant stays the inverted picture. Both BMPs are
  regenerated and committed (as today); `package-device.sh` already installs them.
- **Rationale**: FR-001..FR-004 are about one picture; the script is the existing, pure-Python source of it. The only
  judgment is size and weight, which the device check settles by comparing with the neighbouring icons; the numbers
  above are the starting point, not a requirement.
- **Test** (Test-First, FR-001/FR-003): a host test reads the BMP and asserts that the outermost 10 px ring of the
  canvas is entirely background, that the glyph's bounding box lies within 60–85 % of the canvas, and that the normal and
  focused variants differ; it fails against the current framed icon.
- **Alternatives considered**: *editing the BMP by hand* — not reproducible; *a PNG icon* — the launcher expects BMP
  files named in the `.app` contract (`applications/icons/*.bmp`).

## R-11. Testing the look

- **Decision**: three layers, no new framework.
  1. **Structure tests** (host, offscreen, existing `test_qml_pages` driver): each restyled page and component is loaded
     with the stubs and driven with injected mouse events; they assert *presence and behavior* of the elements in FR-005..
     FR-024 (top bar title is upper case and centred, no `InkButton` with a border remains, toggles change `toggled`,
     dialogs have a close icon and two footer buttons, the layout toggle swaps views and persists, paging by whole rows).
     Existing driver scripts keep finding controls by `text`, which the new controls still have (R-3).
  2. **Whole-app checks**: `--self-test` instantiates every page (already there); a new test walks every QML file for a
     bordered `InkButton` (`bordered: true` or a `border.width` on a text button) so SC-003 cannot regress.
  3. **Device screenshots** (SC-001, SC-002, SC-006): after each slice the walkthrough in `quickstart.md` takes the device's
     own screenshots into `screens/` and compares them with the ten reference ones. Host screenshots
     (`PBNOTES_QML_SHOT`, `PBNOTES_START_PAGE`) are used only while tuning, since the host does not render e-ink grey the
     way the panel does.
- **Rationale**: the structure is testable; "looks like" is a human judgment on the device and is recorded as one.
- **Alternatives considered**: *pixel-diff against the reference screenshots* — the reference contains German labels,
  Roboto and live data (clock, notes), so a diff would fail for reasons unrelated to the look.

## R-12. Slicing and releases

- **Decision**: five slices, each a complete, releasable state (FR-024), each ending with its own MINOR bump,
  CHANGELOG entry and device check (SC-010):

  | Slice | Spec story | Version | Brings |
  |---|---|---|---|
  | 1 | US1 icon | 1.6.0 | frameless launcher icon |
  | 2 | US2 list | 1.7.0 | `Style`, `Icon`, `IconButton`, `TopBar`, SVG set (list part), `list_layout`, restyled list, font gate (R-5), inset gate (R-9) |
  | 3 | US3 dialogs/menus | 1.8.0 | `DialogFrame`, `PopupBubble`, `InkField`, restyled dialogs and menus |
  | 4 | US4 reader/editor | 1.9.0 | `BottomPanel`, `ToolButton`, `InkSlider`, restyled reader and editor |
  | 5 | US5 other screens | 1.10.0 | `InkSwitch`, settings/setup/about/licenses/attach browser |

  A restyled screen next to a not-yet-restyled one is accepted (spec assumption). The shared dialogs keep their names,
  properties and signals (`ConfirmDialog`, `TextEntryDialog`, `ChoiceDialog`, `SelectionMenu`, `SlashMenu`), so a page
  that has not been restyled yet keeps working when slice 3 restyles them. `InkButton` is not touched before it is
  deleted in slice 5, so no unrestyled screen changes look early.
- **Rationale**: matches the clarified answer and makes each slice's device check meaningful.
- **Alternatives considered**: *one big release* — rejected in the clarification.
