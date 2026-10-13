# Device results: Built-in Look (feature 010)

Status as of the implementation on the host. **Nothing in the device columns has been looked at yet**: the five slices were
built and tested on the host in one go, and the 1.10.0 build was installed on the reader for the walkthrough. Quickstart §3
describes every row; record what you see in the *Result* column and take the reader's own screenshots (they land in `screens/`)
for the rows marked 📷.

## What the host already proves

| Criterion | Evidence |
|-----------|----------|
| SC-003 no bordered text button anywhere | `check_ui_style` (scans `qml/` for `InkButton`, `bordered:` and Qt Quick Controls `Button`), plus its self-test; `InkButton.qml` is deleted |
| SC-004 every earlier action still works | the existing suites pass: `qml_editor_flow`, `qml_editor_slash`, `qml_reader_select`, `qml_reader_blocks`, `qml_reader_title`, `qml_file_picker`, and the whole core/bridge/contract/integration set (103 tests). Tests that changed: `editor_flow` (Undo/Redo are now top-bar icons), `reader_select` (taps outside the bubble avoid the new, taller bubble; the footer is a full-width bar), `file_picker` (icons are not previews) |
| SC-007 notes per page / text per page | list: 11 whole rows per page at 1404 × 1872, as in 1.5.0 (`qml_note_list`); a tile page holds at least as many notes; reader: text area 1668 px against 1666 px in 1.5.0 (`qml_reader_page`) |
| SC-008 no animation | no `Behavior`, `*Animation` or non-empty `Transition` in `qml/` (grep recorded in this file's history; `Main.qml` keeps its empty transitions) |
| SC-009 notes, settings, attachments unchanged | no core, store or server change except one new settings row `list_layout` (default `list`); store, migration and account tests extended; no data written by the restyle |
| FR-001..FR-004 launcher icon | `test_launcher_icon` (no frame ring, glyph 73–103 px and centred, anti-aliased, two variants differ) |
| FR-012a, remembered layout | `test_store_open`, `test_account_store`, `test_store_migration_v3`, `test_notes_bridge`, `qml_note_list` |
| Structure of every screen (SC-002, host part) | `qml_ui_style`, `qml_note_list`, `qml_ui_dialogs`, `qml_reader_page`, `qml_editor_bars`, `qml_settings_sync`, `qml_setup_page`, `qml_about_pages`, `qml_file_picker` |

## Device walkthrough (quickstart §3) — open

| Slice | Rows | Result |
|-------|------|--------|
| 1 icon (1.6.0) | 1.1–1.3 📷 SC-001 | open |
| 2 note list (1.7.0) | 2.1–2.10 📷; also the **two gates**: the `font.ui` line of `pbnotes.log` (`roboto=0|1`, then T027 if 0) and whether the system status bar overlays the app (then set `Style.topInset`) | open |
| 3 dialogs and menus (1.8.0) | 3.1–3.5 📷 | open |
| 4 reader and editor (1.9.0) | 4.1–4.6 📷; the `pt` labels against the built-in size control | open |
| 5 remaining screens (1.10.0) | 5.1–5.5 📷; **every screen in all four orientations (SC-006)** | open |

Since the five slices were built together, the per-slice rule of SC-010 (a slice is device-checked before the next starts) was
not followed. If the walkthrough shows a problem, fix it by changing a number in `qml/Style.qml` or one component, never by
adding a feature, and record both the finding and the fix here.

## Known judgment calls to look at on the reader

- The **pressed (focused) icon** is the same picture inverted: a black square with the page drawn in white. If that reads as a
  "frame" next to the built-in highlight, change `down(glyph, True)` in `scripts/make-icons.sh`.
- **Roboto:** the app asks for it by name; if the reader's font list lacks it (`roboto=0` in the log), run task T027 to bundle it.
- **Bar and row sizes** (`Style.barHeight` 112, rows 132, tiles 190, bottom bar 92) were chosen to keep SC-007; the built-in
  top bar is about 135 px. Raise them only while the counts per page still hold.
- The text-size labels (7, 9, 12 pt) come from the pixel size at 300 ppi; the built-in control shows 11 pt for its own default,
  so compare the look of the text at the same label rather than the number.
- The "/" menu keeps **two columns** (twelve entries do not fit one column without scrolling).
