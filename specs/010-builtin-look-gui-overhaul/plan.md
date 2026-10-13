# Implementation Plan: Built-in Look (GUI Overhaul)

**Branch**: `feature/010-builtin-look-gui-overhaul` | **Date**: 2026-10-13 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/010-builtin-look-gui-overhaul/spec.md`

## Summary

Every screen of the app takes on the look of the reader's own apps (the ten reference screenshots), and the launcher
icon loses its frame. It is a **presentation change on top of the existing three layers** (Qt-free core, thin Qt bridge,
QML) with **no core change, no new library, no schema change, no server change**. The one persisted addition is the
note list's remembered layout (`list_layout`, `list`|`tile`). It ships in **five slices**, each releasable on its own
(FR-024) and each ending with a MINOR version and a device check:

1. **Launcher icon** (1.6.0): `scripts/make-icons.sh` draws the page glyph without the outer frame, anti-aliased,
   sized like the built-in icons; a host test reads the BMPs. No QML.
2. **Note list** (1.7.0): the foundation (a `Style` singleton, an SVG icon set shown by `Icon`, `IconButton`, `TopBar`),
   then the list itself: top bar, filter/sort row with drop-down labels, count line, rows, the **Tile layout** with its
   toggle and remembered setting. Two device gates are settled here: the UI typeface (R-5) and the status-bar inset (R-9).
3. **Dialogs and menus** (1.8.0): `DialogFrame` (heavy rounded outline, title + close over a rule, footer of
   full-width upper-case buttons, dimmed page) behind `ConfirmDialog`, `TextEntryDialog`, `DateTimePicker`,
   `EmojiPicker`; `PopupBubble` (outline + pointer) behind `SelectionMenu`, `SlashMenu` and `ChoiceDialog`. Their
   interfaces stay, so the editor's "/" menu and selection logic and their tests are untouched.
4. **Reader and editor** (1.9.0): top bars with icon-only universal actions, a bottom panel of icon + label tools
   (reader: *Text size* with a ring-knob slider and round minus/plus; editor: *Attach*, *Category*, *Delete* above the
   keyboard), a `n / total` indicator, the title as a labelled field.
5. **Remaining screens** (1.10.0): Settings (a pill switch for Auto Sync), setup, About, license pages and the attach
   browser get the same top bar, rows and fields; the old bordered `InkButton` is deleted, so SC-003 holds by construction.

The resolved unknowns are in `research.md` (R-1 … R-12); the interfaces are in `contracts/ui-components.md`.

## Technical Context

**Language/Version**: QML (Qt Quick 6.8) for all UI work; C++17 only for the `list_layout` bridge property and (R-5)
one start-up log line / font choice in `app/main.cpp`; Python 3 (stdlib only) for `scripts/make-icons.sh`.

**Primary Dependencies**: none added. The Qt SVG image plugin is already on the reader (`svg` in the logged
`image.formats` list of 1.5.0). Roboto is bundled **only if** the device probe (R-5) shows it missing.

**Storage**: one new row in the existing `settings` table (`list_layout`, default `list`), validated like the others.
No schema change, no migration. Nothing else is written.

**Testing**: the existing harness. Host unit tests (CTest `tests/unit`), bridge tests (`tests/bridge`), offscreen QML
tests with injected mouse events (`tests/qml`, `stubs.qml`), the app `--self-test`, a new source scan for bordered text
buttons, and a new host test for the launcher BMPs. The device walkthrough (`quickstart.md` §3) judges "looks like".

**Target Platform**: PocketBook InkPad One (Qt 6.8.x runtime from the SDK build, 1404 × 1872 panel at 300 ppi) and the
Linux x86-64 host — unchanged.

**Project Type**: single application with a host-testable core library — unchanged.

**Performance Goals**: no screen is slower to open or to page than in 1.5.0 (SC-007's per-page counts are the proxy).
An icon is a cached `Image`; opening a dialog or a bubble is one `visible` change and one redraw (SC-008, FR-011).

**Constraints**: monochrome panel — state by weight, fill, outline, never color (FR-008); no animation, fade or
transition; touch targets no smaller than today's 78 px buttons (FR-023); the keyboard must keep the editor's field
and panel reachable; four orientations (FR-022); the editor's placement code for menus is not changed.

**Scale/Scope**: 17 QML files restyled or retired, ~12 new small QML components, about 50 SVG files (a few hundred bytes
each), 1 setting + 1 bridge property (≈ 15 lines + its tests), 1 script rewritten (≈ 40 lines), 4–6 new QML test drivers
and 6 existing ones updated.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Gate (constitution) | Pre-research | Post-design | Evidence |
|---------------------|--------------|-------------|----------|
| I. Test-First | PASS | PASS | Every behavior in the stories is a testable function of the QML tree and the stubs: element presence per screen, toggles, paging by whole rows, remembered layout, dialog structure, the launcher BMP geometry. `tasks.md` must order each failing test before its code (updated drivers first where a control changes from text button to icon) |
| II. Simplicity (YAGNI) | PASS with justified items | PASS with justified items | Five items in Complexity Tracking. Rejected: a custom Qt Quick Controls style plugin, an icon font, bundled Roboto unless the device lacks it, a continuous text-size range, a text excerpt on tiles, a settings screen for the look, theme/dark mode, our own status bar |
| III. Security & Privacy | PASS | PASS | No network, no new data, no new input. Icons are static assets in the binary. The new log line carries a font family name and a 0/1 flag only; no note, path or account data (R4/R6 of `check-constitution.sh` unaffected) |
| IV. Integration tests at device boundaries | PASS | PASS | No boundary format changes (database rows, server requests, attachment files are unchanged and covered by the existing suites, which must pass untouched). The launcher BMP format is a device boundary: it gets a contract test |
| V. Host-testable core | PASS | PASS | `core/` is not touched. All new code is QML and bridge glue, runs offscreen on the host without InkView |
| VI. No silent data loss | PASS | PASS | The restyle moves buttons but keeps every confirmation: discard-changes and delete dialogs remain, dirty-leave still asks, *Delete* stays two steps (tool, then confirm). FR-021 and SC-009 forbid any stored change; the data layer is not edited |
| Stack: C++17, Qt 6.8 Quick/QML, CMake, libcurl, SQLite | PASS | PASS | No stack change |
| SDK location only from `POCKETBOOK_SDK` / `-DSDK_PATH` | PASS | PASS | No build path added; new files are added to `qml.qrc` and `bridge/CMakeLists.txt` lists only |
| Both device and host build targets | PASS | PASS | The qrc is shared by both builds; the manual ARM rcc path is exercised by a first spike (R-2) |
| Licensing: InkFeed attributed; notes-android reference-only | PASS | PASS | Icons are drawn for this project (own work, MIT); the device's built-in icon files and screenshots are **not** copied into the repository. Nothing from notes-android. If Roboto must be bundled (R-5): Apache-2.0 text, NOTICE section, `licenses/manifest.txt` entry, packaging line, About list — the Noto Emoji pattern |
| Release version | PASS | PASS | Each slice ends with a MINOR bump (1.6.0 … 1.10.0) and a CHANGELOG entry as its last task, checked by `scripts/check-version-bump.sh` |

No violations block planning. The reference screenshots stay on the reader owner's device; the spec describes them in words and
the repository does not contain the manufacturer's artwork.

## Project Structure

### Documentation (this feature)

```text
specs/010-builtin-look-gui-overhaul/
├── plan.md              # This file
├── research.md          # Phase 0: decisions R-1 .. R-12
├── data-model.md        # Phase 1: the list_layout setting, transient state, Style constants
├── quickstart.md        # Phase 1: host tests and the five-slice device walkthrough
├── contracts/
│   └── ui-components.md # NotesBridge.listLayout, Style, the components, per-screen contents, assets, launcher icon
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit-tasks — not created here)
```

No `contracts/core-library.md` or API contract: neither the core nor the server interface is touched.

### Source Code (repository root)

```text
core/src/note_store.cpp              # + list_layout in setting_defaults / valid_setting            (slice 2)
core/include/pbnotes/note_store.h    # settings doc: list_layout {list,tile}
bridge/
├── include/notes_bridge.h           # + listLayout property                                      (slice 2)
└── src/notes_bridge.cpp             # listLayout / setListLayout
app/main.cpp                         # UI font choice + one log line (R-5)                        (slice 2)
scripts/make-icons.sh                # frameless, anti-aliased launcher icon                      (slice 1)
res/icons/pbnotes.bmp, pbnotes_f.bmp # regenerated
qml/icons/*.svg                      # the icon set (contracts §5)                                (slice 2, grown per slice)
qml/
├── qmldir, Style.qml                # the singleton (R-2)                                        (slice 2)
├── Icon.qml, IconButton.qml, ToolButton.qml, TopBar.qml, BottomPanel.qml, RowDivider.qml   (slices 2, 4)
├── DialogFrame.qml, FooterButton.qml, PopupBubble.qml, InkField.qml                         (slice 3)
├── InkSwitch.qml, InkSlider.qml                                                              (slices 4, 5)
├── ConfirmDialog.qml, TextEntryDialog.qml, ChoiceDialog.qml, DateTimePicker.qml, EmojiPicker.qml,
│   SelectionMenu.qml, SlashMenu.qml, PageButtons.qml        # restyled, interfaces kept       (slices 2–3)
├── InkButton.qml                    # deleted at the end of slice 5
├── Main.qml                         # unchanged apart from a possible `Style` import
├── qml.qrc                          # + the new files and the icons (alias icons/<name>.svg)
└── pages/
    ├── NoteListPage.qml             # top bar, filter/sort row, count, List/Tile, popups         (slice 2)
    ├── ReaderPage.qml, EditorPage.qml      # bars, panels, labelled fields                       (slice 4)
    └── SettingsPage.qml, SetupPage.qml, AboutPage.qml, LicenseTextPage.qml, ThirdPartyPage.qml,
        FilePickerPage.qml, ImagePage.qml                                                         (slice 5)
tests/
├── unit/                            # test_store_open (+ list_layout); test_launcher_icon (new)  (slices 1–2)
├── bridge/                          # test_notes_bridge (+ listLayout)                           (slice 2)
├── qml/                             # ui_style, note_list, ui_dialogs, reader_page, setup_page,
│                                    #   about_pages (new); editor_*, reader_*, settings_sync, file_picker updated;
│                                    #   stubs.qml (+ listLayout)
├── scripts or unit/                 # no_boxed_buttons source scan                               (slice 5)
└── CMakeLists.txt                   # the new tests
VERSION, CHANGELOG.md                # 1.6.0 … 1.10.0, one per slice (last task of each slice)
licenses/manifest.txt, NOTICE, scripts/package-device.sh   # only if Roboto is bundled (R-5)
```

**Structure Decision**: the repository's layout is kept; no new module or library; the icons are a new directory `qml/icons/` beside the QML that loads them.
Look constants and shapes live in `qml/` as small single-purpose components used by every page; rules about *what is
shown* stay where they are (the bridge and the existing page logic), so the restyle never edits behavior. The
editor's text-selection and "/"-menu placement code, the reader's block rendering and selection code, paging
(`PageButtons.pageBy`) and the attach browser's listing logic are explicitly left alone.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| C-1. A QML singleton `Style` with a `qmldir` | FR-005..FR-011 need one consistent look across 17 files and tuning against the device screenshots in one place | Copies of `#111111`, `border.width: 2`, `font.pixelSize: 38` in every file drift and make each tuning round a 17-file edit; a bridge context property would put presentation in C++ |
| C-2. about 50 SVG icon files and an `Icon` component | FR-006 requires line-art controls in one stroke weight; curves (star, gear, magnifier, undo) cannot be drawn from rectangles | Unicode symbols differ per font and weight; an icon font needs a font tool chain and a generated binary; PNGs need a second asset set per size |
| C-3. About twelve new QML components | Ten screens and seven dialogs/menus share the same top bar, outline, footer, bubble, switch and slider; the spec's FR-014/FR-015 require the same structure everywhere | Restyling each page by hand repeats the heavy outline and the footer five times and cannot guarantee SC-002/SC-003; a custom Qt Quick Controls style would change the manual ARM build for a dozen controls |
| C-4. A new persisted setting `list_layout` | FR-012a: the Tile layout must survive leaving the page and restarts | Holding it in QML loses it at restart; sharing `picker_view` couples two unrelated screens |
| C-5. Possibly bundling Roboto (Apache-2.0) | FR-010 matching typography, if the reader's font database lacks Roboto to the app | Using the default font leaves FR-010 unmet when it is not Roboto-like; the plan bundles **only after** the device probe shows it is needed (R-5) |
