# Baselines of 1.5.0 (feature 010, T002)

Measured on the host with a throwaway QML driver (not committed) run through `build-host/tests/test_qml_pages` in a
1404 × 1872 window, with the stubs of `tests/qml/stubs.qml` (60 notes). The panel is 1404 × 1872.

| What | Value | How |
|------|-------|-----|
| Note list: height of the list view | 1527 px | `ListView` of `qml/pages/NoteListPage.qml` |
| Note list: whole rows per page | **11** (`floor(1527 / 132)`) | SC-007 list baseline |
| Reader: height of the text area | **1666 px** | the item with `objectName: "flick"` in `qml/pages/ReaderPage.qml` (y = 112) |
| Launcher icon (framed): dark pixels bounding box | x 6..115, y 6..115 (110 × 110 px) | `res/icons/pbnotes.bmp`, pixels below grey 192 |

Later tasks compare against these: the 1.7.0 list must show at least 11 whole rows per page (T018), the 1.9.0 reader text
area must be at least 1666 px high (T037, SC-007).
