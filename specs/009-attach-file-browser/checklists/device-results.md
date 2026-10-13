# Device results: Attach File Browser (feature 009)

**Device**: PocketBook InkPad One, release package `pbnotes-inkpad-one-1.5.0` installed 2026-10-13.

## Outcome

The reader's owner tried the new attach browser on the device and reported that it works ("Works like a
charm"). That covers the curated seven-folder top level, the List/Tile toggle and the image previews in
Tile view.

## Not recorded

The 12 rows of `quickstart.md` §3 were not logged one by one, and no numbers were taken for SC-003 (tiles
per page against list rows), SC-005 (previews within 10 s) or SC-006 (five minutes of browsing). The tile
sizes (270 / 250 / 170 in `qml/pages/FilePickerPage.qml`) stay as they are; on the host the tests show 30
tiles against 15 list rows at the panel's width.
