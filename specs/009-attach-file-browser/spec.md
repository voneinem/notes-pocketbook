# Feature Specification: Attach File Browser (Curated Folders, List and Tile View, Previews)

**Feature Branch**: `feature/009-attach-file-browser`

**Created**: 2026-10-13

**Status**: Draft

**Input**: User description: "Edit -> attach -> Only show folders: Audio Books, Downloads, Music, Ma pictures, Notes, Photo, screens. Do not show the root files (Autorun.inf, install.md). Support List and Tile view - toggle icon on top. Show image preview in tile view."

## Clarifications

### Session 2026-10-13

- Q: Is the pictures folder named "Ma pictures" or "My pictures"? → A: "My pictures" (only that name is matched; "Ma pictures" in the original request was a typo).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Start the file browser in a clean list of useful folders (Priority: P1)

A reader editing a note taps "Attach". The file browser opens and, at its top level, shows only
the seven folders where the reader keeps things worth attaching: Audio Books, Downloads, Music,
My pictures, Notes, Photo and screens. The system files that sit next to them on the device
(for example Autorun.inf and install.md) and any other folder are not shown. The reader opens a
folder, sees everything inside it, picks a file and confirms, exactly as before.

**Why this priority**: Today the browser opens on the device's whole top level, which is mostly
clutter: system files and folders that never hold anything to attach. Hiding them removes the
search for the right place and the chance of tapping a file that is not meant to be touched. It
is also the smallest slice that is useful on its own, so it comes first.

**Independent Test**: On a device whose top level holds the seven folders, Autorun.inf,
install.md and at least one other folder, edit a note and tap "Attach". Verify that exactly the
seven folders are listed, that neither file nor the other folder is shown, then open "Photo",
pick an image, confirm, and verify it is attached to the note.

**Acceptance Scenarios**:

1. **Given** the reader taps "Attach" in the editor, **When** the file browser opens, **Then** the
   top level lists only those of the seven named folders that exist on the device, and no files
   and no other folders.
2. **Given** the top level is shown, **When** the reader opens one of the folders, **Then** all of
   its subfolders and files are listed, not only those with certain names or types.
3. **Given** the reader is inside one of the seven folders (or a folder below it), **When** they
   choose "Up" repeatedly, **Then** they arrive back at the top level of seven folders, and "Up"
   is unavailable there: the reader cannot go above it to see the device's root.
4. **Given** one or more of the seven folders does not exist on the device, **When** the top level
   is shown, **Then** only the existing ones are listed and nothing else takes their place.
5. **Given** none of the seven folders exists, **When** the top level is shown, **Then** the
   reader sees a clear message that there is nothing to browse, and "Cancel" still works.
6. **Given** the reader opens the browser a second time, **When** it appears, **Then** it starts at
   the top level again, not inside the last folder.

---

### User Story 2 - Switch between List and Tile view (Priority: P2)

At the top of the file browser the reader finds an icon that toggles between two layouts. The
List view is today's: one row per entry with name and size. The Tile view shows the entries as a
grid of tiles, several per row, each with a symbol and a name, so more entries fit on a page. The
reader switches at any time and stays in the same folder, and the app remembers the choice for
next time.

**Why this priority**: A grid shows more entries per page, which matters on an e-ink screen
where a page turn is slow, and it is the layout that makes pictures practical. But it is a
convenience on top of being able to find and attach a file, so it follows the folder
restriction.

**Independent Test**: Open the browser inside a folder with at least thirty entries. Tap the
toggle icon: the entries are laid out as tiles and more of them fit on one page than in the
list. Tap it again: the list returns, in the same folder. Leave the browser, open it again and
verify the last-used layout is still in effect.

**Acceptance Scenarios**:

1. **Given** the browser is in List view, **When** the reader taps the toggle icon at the top,
   **Then** the same folder is shown as tiles, and the icon now shows that a tap returns to the
   list.
2. **Given** the browser is in Tile view, **When** the reader taps the toggle icon, **Then** the
   same folder is shown as a list again.
3. **Given** the reader switches the layout, **When** it changes, **Then** the current folder, the
   path line and any message stay as they were, and the reader is not sent back to the top level.
4. **Given** the reader chose a layout, **When** they leave the browser and open it again, even
   after restarting the app, **Then** it opens in the layout they chose last; on first use it is
   the List view.
5. **Given** Tile view is shown, **When** the reader taps a folder tile, **Then** the folder opens;
   **When** they tap a file tile, **Then** the same size check and "Attach this file?" question
   appear as in List view, with the same outcome.
6. **Given** a folder holds more entries than fit on one page, **When** the reader uses the page
   buttons, **Then** the browser moves by whole pages in both layouts, with no partly visible
   rows or tiles at the page edge.
7. **Given** an entry has a long name, **When** it is shown as a tile, **Then** the name is
   shortened to fit the tile, never overlapping a neighbor, and the full name is shown in the
   confirmation question when the file is picked.

---

### User Story 3 - See what a picture is before attaching it (Priority: P3)

In Tile view every image file shows a small preview of the picture itself, so the reader
recognizes the right photo or screenshot by sight instead of reading file names such as
"IMG_20261003_141522". Files that are not images and folders show a symbol for their kind
instead. In List view nothing changes.

**Why this priority**: Previews are what make the Tile view worth having for photos and
screenshots, but the browser is fully usable without them: the name, the size and the
confirmation still identify the file. They also carry the most risk for speed and memory on the
device, so they come last and must not hold up the rest.

**Independent Test**: Open "Photo" or "screens", containing a mix of images, a document and a
subfolder, in Tile view. Verify each image shows a recognizable preview, the document and the
subfolder show a symbol, and the page can be used (scrolling by page, opening a folder, picking
a file) while previews are still appearing. Switch to List view and verify no previews appear.

**Acceptance Scenarios**:

1. **Given** Tile view shows a folder with image files, **When** the page is displayed, **Then**
   each image tile shows a preview of that image, scaled to fit the tile, in grayscale on the
   device's screen, with the picture's proportions kept.
2. **Given** Tile view shows non-image files and folders, **When** the page is displayed, **Then**
   each shows a symbol that tells a folder from a file, plus the name, and no preview is
   attempted.
3. **Given** previews are still being prepared, **When** the reader taps a tile, uses the page
   buttons, goes "Up" or toggles to List view, **Then** the browser responds without waiting for
   the previews, and previews for entries that are no longer shown are dropped.
4. **Given** an image is very large, damaged or in a format the device cannot decode, **When** its
   tile is shown, **Then** the tile shows the file symbol instead, the name and size still work,
   and the reader can still pick the file; nothing else on the page is affected.
5. **Given** the reader returns to a page whose previews were shown before, **When** it is shown
   again, **Then** the previews appear without a noticeable delay, and the number of previews
   kept does not grow without limit as the reader browses.
6. **Given** a preview is shown, **When** the reader picks the file, **Then** the original file is
   attached unchanged, not the reduced preview, and the file on the device is not modified.
7. **Given** the reader is in List view, **When** a folder is shown, **Then** no previews are
   prepared at all.

---

### Edge Cases

- A folder holds hundreds of files, most of them images: the browser stays usable, only the
  tiles on the current page get previews, and the reader can leave the folder at any moment.
- A folder name on the device differs from the listed names only by upper and lower case (for
  example "photo" or "Screens"): it counts as the named folder and is shown.
- A listed name exists on the device as a file, not a folder: it is not shown.
- A listed folder is a link to a folder elsewhere on the device: it is shown and opens like any
  other folder, and the reader can come back "Up" to the top level.
- A folder cannot be read (permission or media error): the existing message for an unreadable
  folder is shown and "Up" and "Cancel" still work.
- Files whose names start with a dot or that are hidden: they stay unlisted inside the folders, as
  today; the only new restriction is at the top level.
- An image is larger than the attachment size limit: its tile shows the preview, but picking it
  is refused with the existing message, as in List view.
- The device's storage is removed or unmounted while the browser is open: the folder is shown as
  empty or unreadable with the usual message, and the app does not crash.
- The reader rotates the device while the browser is open: the layout adapts, the folder and the
  chosen view stay, and tiles per row follow the new width.
- Many images with the same file name in different folders: each preview belongs to its own file;
  none is shown for another.
- Two tiles are tapped in quick succession: only one action happens; a folder is not opened
  twice.

## Requirements *(mandatory)*

### Functional Requirements

**Top level**

- **FR-001**: When the file browser opens, it MUST show at its top level only the folders named
  Audio Books, Downloads, Music, My pictures, Notes, Photo and screens, and only those that
  exist on the device.
- **FR-002**: The top level MUST NOT show any file (including Autorun.inf and install.md) and
  MUST NOT show any folder other than the seven named ones.
- **FR-003**: Folder names MUST be matched without regard to upper and lower case and MUST match
  the whole name, not part of it.
- **FR-004**: Inside any of the seven folders, and in every folder below them, the browser MUST
  list all subfolders and files without filtering.
- **FR-005**: "Up" MUST lead from a first-level folder back to the top level of named folders and
  MUST NOT be available at the top level; the reader MUST NOT be able to reach the device's
  root listing through the browser.
- **FR-006**: The browser MUST always open at the top level. If no named folder exists, it MUST
  show a clear message that there is nothing to browse, and "Cancel" MUST remain available.
- **FR-007**: Choosing a file MUST work as before: a file over the size limit is refused with a
  message, otherwise its name and size are shown and the reader confirms or cancels; confirming
  attaches a copy and leaves the original unchanged. This applies in both layouts.

**List and Tile view**

- **FR-008**: The top of the browser MUST show an icon that toggles between List view and Tile
  view, and the icon MUST show which action a tap performs (a grid symbol while the list is
  shown, a list symbol while tiles are shown).
- **FR-009**: List view MUST keep today's behavior: one row per entry with a kind label, name and
  size, and no previews.
- **FR-010**: Tile view MUST show the entries of the current folder as a grid of equal-sized
  tiles, each with a preview or symbol and the entry's name, with more entries per page than the
  list shows.
- **FR-011**: Switching the layout MUST keep the current folder, path line and message, and MUST
  NOT reset the position to the top level.
- **FR-012**: The chosen layout MUST be kept for later visits to the browser, including after the
  app is restarted. The default on first use MUST be List view.
- **FR-013**: In both layouts, entries MUST be ordered the same way as today (folders before files
  and each group in name order), so switching the layout does not reorder them.
- **FR-014**: In both layouts, paging MUST move by whole pages (page buttons, no kinetic
  scrolling), and no row or tile may be cut off at the page edge.
- **FR-015**: Long names MUST be shortened to fit their row or tile; the full name MUST be shown in
  the confirmation question.
- **FR-016**: Both layouts MUST be operable by touch only, with tap targets large enough for
  reliable use on the e-ink screen, and MUST adapt to the device's orientation.

**Previews**

- **FR-017**: In Tile view, every file that is an image of a kind the app can show in a note MUST
  display a preview of the image, with its proportions kept, scaled to fit the tile.
- **FR-018**: Folders and files that are not images MUST display a symbol for their kind, and no
  preview MUST be attempted for them.
- **FR-019**: An image that cannot be decoded (too large, damaged, unsupported) MUST fall back to
  the file symbol; the entry MUST remain usable and picking it MUST still work.
- **FR-020**: Previews MUST be prepared only for the tiles of the page currently shown, MUST NOT
  block touch handling, paging, folder changes or the layout toggle, and MUST be abandoned for
  entries that are no longer shown.
- **FR-021**: The memory used for previews MUST stay bounded however many folders and pages the
  reader browses.
- **FR-022**: A preview MUST NOT change, move or delete the original file, and attaching a file
  MUST attach the original, not the preview. Previews MUST NOT be written to the device's
  storage as files visible to the reader, and MUST NOT be sent anywhere.
- **FR-023**: In List view no previews MUST be prepared.

**Data safety and privacy**

- **FR-024**: Browsing MUST be read-only: the browser MUST NOT create, rename, move or delete
  anything on the device.
- **FR-025**: Logs MUST NOT contain file or folder names or other content of the reader's files.

### Key Entities

- **Browsable top level**: The fixed set of seven folder names that the browser offers as its
  starting points. It exists only inside the browser; nothing on the device is moved or renamed
  to create it.
- **Entry**: A folder or a file shown in the browser, with name, kind (folder, image, other
  file) and, for files, size.
- **Layout choice**: The reader's last-used view (List or Tile), kept between visits to the
  browser and between app runs.
- **Preview**: A reduced picture of an image file, kept only in memory while its tile can be seen
  or is likely to be seen next, and discarded afterwards.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On a device with the seven folders, the system files and at least one other folder
  at its top level, 100% of browser openings list exactly the seven folders and no other entry.
- **SC-002**: A reader can find a photo in the "Photo" folder in Tile view, recognize it by its
  preview and attach it in no more than 5 taps after pressing "Attach" (open folder, optionally
  page, tap file, confirm).
- **SC-003**: In Tile view at least twice as many entries fit on one page as in List view.
- **SC-004**: Switching between List and Tile view takes under 2 seconds on the device, and the
  reader is in the same folder afterwards in 100% of switches.
- **SC-005**: For a page of tiles in a folder of ordinary photos, every tile is usable (can be
  tapped, paged away from) at once, and at least 90% of image tiles show their preview within
  10 seconds on the device.
- **SC-006**: Browsing a folder with 500 images for 5 minutes (paging, opening and leaving
  folders, toggling the layout) does not make the app unresponsive, slow noticeably, or run out
  of memory.
- **SC-007**: In 100% of cases where an image cannot be previewed, the reader can still attach
  that file, and in 100% of attachments the file attached is identical to the original.
- **SC-008**: Attaching a file works exactly as before this feature in every case the reader
  could attach before (size limit, confirmation, cancel, discarding on leaving the editor),
  confirmed by the existing attachment checks still passing.

## Assumptions

- The folders are those at the top level of the device's user storage, which is where the
  browser starts today; "Autorun.inf" and "install.md" are two examples of files found there.
- The folder names are fixed and are matched exactly as listed in FR-001; "My pictures" is the
  spelling confirmed by the user (the original request wrote "Ma pictures"). They are not
  editable in the app, because no setting for them was asked for.
- The restriction applies only to the top level; inside the seven folders everything is shown,
  because the reader may keep files in any subfolder.
- "Image" means the formats the app can already show inside a note (the picture formats used by
  attachments in feature 002); other pictures show the file symbol.
- The layout choice is stored with the app's other per-device preferences and is not synchronized
  to the server.
- The size limit, the confirmation question, the copy-on-attach behavior and the page buttons are
  the existing ones from feature 002 and are not changed.
- Previews are grayscale on the device because the screen is; no color handling is specified.
- Only the file browser opened by "Attach" in the editor is in scope; other screens that show
  files, a way to sort or search in the browser, and selecting several files at once are out of
  scope.
- Depends on feature 002 (attachments) and its file browser, which this feature changes.
