# Feature Specification: Rich Note Content (Links, Tables, Attachments, Emojis)

**Feature Branch**: `002-rich-note-content`

**Created**: 2026-10-10

**Status**: Draft

**Input**: User description: "Add support for hyperlinks to open in the default web browswer. Support for markdown tables and attachments. Support for emojis."

## Clarifications

### Session 2026-10-10

- Q: When should the app download a note's attachments to the device? → A: Only on request. Sync
  never downloads attachments. A note shows each attachment as a placeholder (name and size) with
  a Download action; once downloaded, the file stays on the device and works offline.
- Q: Should the app ask for confirmation before opening a tapped link? → A: Only when the link's
  displayed text differs from its destination (changed 2026-10-10 from "never" to this). A link
  whose text is the address itself (a bare address, or a title that is the same address) opens at
  once; any other link first shows the real destination and asks for confirmation.
- Q: Should tapping a downloaded image show it larger? → A: Yes. A tap opens the image full screen,
  fitted to the whole screen, with a Back control. There is no zoom or panning.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Follow a link from a note (Priority: P1)

A reader is reading a note that contains a web address, either as a titled link or as a plain
address typed into the text. They tap it and the page opens in the device's default web browser;
if the link's text hides a different address, they first see the real address and confirm. When
they come back to the app, the note is still open where they left it.

**Why this priority**: Links in notes (articles to read later, references, ticket numbers) are the
most common reason a note points outside itself, and today they cannot be followed at all. This is
the smallest slice that delivers value and needs no changes to how notes are stored or synced.

**Independent Test**: Open a note containing a titled link and a bare address. Tap the bare address
and verify the browser opens at once; tap the titled link, check that the real address is shown,
confirm, and verify the browser opens the right page; return to the app and verify the note is shown
unchanged at the same position.

**Acceptance Scenarios**:

1. **Given** a note contains a titled link, **When** the user opens it, **Then** the link is
   visibly distinguishable from surrounding text (for example underlined) and shows its title as
   the link text.
2. **Given** a note contains a bare web address in its text, **When** the user opens it, **Then**
   the address is also shown as a link.
3. **Given** the user taps a link whose displayed text is the address itself (a bare address, or a
   titled link whose text is the same address), **When** the tap is registered, **Then** the
   destination opens in the device's default web browser at once, with no confirmation step.
4. **Given** the user taps a link whose displayed text differs from its destination, **When** the
   tap is registered, **Then** the app shows the real destination address and asks for
   confirmation before leaving; declining leaves the note unchanged, and confirming opens the
   destination in the default web browser.
5. **Given** the user followed a link, **When** they return to the app, **Then** it shows the same
   note at the same reading position.
6. **Given** the device has no network connection, **When** the user follows a link, **Then** the
   app (or the browser) tells the user that the page cannot be loaded; the note and the app are
   unaffected.
7. **Given** a link points to something other than a web address (for example a file path or an
   unknown scheme), **When** the note is displayed, **Then** it is shown as plain text and cannot
   be followed.
8. **Given** a link's displayed title is different from its destination, **When** the user
   confirms, **Then** the real destination (not the title) is what opens.

---

### User Story 2 - Read tables in notes (Priority: P1)

A reader has a note with a Markdown table (a schedule, a comparison, a packing list with
quantities). Instead of rows of pipe characters and dashes, they see a proper grid with a distinct
header row, columns lined up, and every cell readable on the narrow e-ink screen.

**Why this priority**: Tables written elsewhere (web, desktop, phone) currently show up as
unreadable lines of pipes on the device, which makes those notes effectively unusable there.
Together with links it makes the reader trustworthy for real-world notes.

**Independent Test**: Open a note with a three-column table whose header, alignment markers, and a
few body rows are present. Verify a grid is shown, the header is distinct, column alignment
(left/center/right) follows the markers, and nothing is cut off. Open the note in the editor and
verify the original table text is intact.

**Acceptance Scenarios**:

1. **Given** a note contains a valid table (header row, separator row, body rows), **When** the
   user opens it, **Then** it is shown as a grid with the header row visibly distinguished and
   cell borders or row separators that remain legible in black and white.
2. **Given** the table's separator row marks columns as left-, center- or right-aligned, **When**
   the table is shown, **Then** the cell contents follow those alignments.
3. **Given** a table is wider than the screen, **When** the note is opened, **Then** the columns
   share the available width and long cell text wraps within its cell, so no part of the table is
   reachable only by sideways scrolling.
4. **Given** a cell contains emphasis, a link, or an emoji, **When** the table is shown, **Then**
   the cell displays it the same way as in normal text (and a link in a cell can be followed as in
   User Story 1).
5. **Given** text that looks like a table but is malformed (for example a missing separator row),
   **When** the note is opened, **Then** it is shown as ordinary text, never as a broken or
   partially drawn grid.
6. **Given** the user opens a note with a table in the editor, **When** they edit and save, **Then**
   the table remains plain text exactly as typed; no table-specific editing is required.
7. **Given** a table is long, **When** the user pages through the note, **Then** a row is never
   split across two pages.

---

### User Story 3 - See and keep emojis (Priority: P2)

A reader whose notes contain emojis (a smiley in a to-do, a flag next to a place name, a status
marker in a heading) sees them in the note, in the list, and in titles, and none of them are lost,
garbled, or replaced when they edit or sync a note.

**Why this priority**: Emojis are common in notes created on phones and are otherwise shown as
empty boxes or question marks. Correct preservation matters more than pretty rendering; losing or
corrupting them on sync would be data damage, which makes this the safety floor of the feature.

**Independent Test**: Create a note on the server containing a mix of emojis (simple ones, flags,
skin-tone variants, combined sequences) in the title and body. Sync, and check the list, the
reader and the editor. Edit one unrelated word, save, sync, and verify the server text still has
the identical emojis.

**Acceptance Scenarios**:

1. **Given** a note contains emojis, **When** the user opens it, **Then** each emoji the device can
   draw is shown as a recognizable glyph in the reader, with line spacing and layout unaffected.
2. **Given** an emoji the device cannot draw, **When** the note is displayed, **Then** a clear
   stand-in symbol is shown in its place, the surrounding text is unaffected, and the original
   emoji is kept in the note.
3. **Given** a note's title or first line contains an emoji, **When** the note list is shown,
   **Then** the emoji appears in the list entry.
4. **Given** a note with emojis is edited on the device and saved, **When** it syncs, **Then** the
   server holds exactly the same emojis for all the text the user did not change, and the
   reverse holds for edits made on the server.
5. **Given** the on-screen keyboard offers emojis, **When** the user enters one in the editor,
   **Then** it is stored, shown, and synced like any other character.
6. **Given** the user searches the note list for an emoji, **When** a note contains it, **Then**
   the note appears in the results.
7. **Given** an emoji is made of several parts (a flag, a skin-tone variant, a family or other
   combined sequence), **When** the note is displayed, **Then** it is shown as one symbol (or one
   stand-in), never split into its parts, and it is never cut in half by line wrapping,
   truncation in the list, or the cursor in the editor.

---

### User Story 4 - View attachments in a note (Priority: P2)

A reader whose note has files attached on another device (a photo of a receipt, a scanned page,
a PDF) sees where each attachment sits in the note, with its name and size. They choose which ones
to download; downloaded images appear in place and other files can be opened. Downloaded
attachments stay on the device and work offline.

**Why this priority**: Notes that depend on an attached image or document lose their meaning when
the attachment is missing; but it is the largest piece of work in this feature (storage,
download, display) and depends on the reader improvements above.

**Independent Test**: Attach an image and a PDF to a note on the server. Sync and open the note:
both appear as placeholders with name and size, and nothing was downloaded. Download the image
and the PDF, switch off Wi-Fi, and reopen the note. The image is shown at its place in the text,
and the PDF can be opened.

**Acceptance Scenarios**:

1. **Given** a note references an attachment that is not downloaded, **When** the user opens the
   note, **Then** a placeholder is shown at the attachment's place in the text with its file name,
   size and a Download action; the note text is fully readable.
2. **Given** the user chooses Download and the device has a network connection, **When** the
   download finishes, **Then** an image is shown at its place in the text, scaled to fit the
   screen width and rendered legibly in grayscale, and any other file is shown as an entry with
   its name and size that the user can open with the device's matching viewer (if none exists,
   the user is told so).
3. **Given** a note has attachments, **When** a sync runs, **Then** no attachment is downloaded by
   the sync; **Given** an attachment was downloaded earlier, **When** the device has no network,
   **Then** it can still be viewed or opened.
4. **Given** the user chooses Download with no network, or the download fails (connection lost,
   file over the size limit, removed on the server), **When** this happens, **Then** the user is
   told the reason, the placeholder remains with the Download action available where it makes
   sense, and the note text is unaffected.
5. **Given** the reference to an attachment disappears from a note on the server, or the note is
   deleted there, **When** a sync runs, **Then** the downloaded copy is removed from the device
   too, unless the note has unsynced local changes (consistent with the rule for notes).
6. **Given** a note is edited and saved on the device, **When** it syncs, **Then** the text that
   references attachments stays intact, so the attachments are still attached on the server.
7. **Given** a downloaded image is shown in a note, **When** the user taps it, **Then** it opens
   full screen, fitted to the whole screen (in the current orientation), and a Back control returns
   to the note at the same reading position.
8. **Given** the device is low on storage, **When** the user downloads an attachment, **Then** the
   download is stopped with a visible message, and no note text or already-downloaded content is
   lost.

---

### User Story 5 - Add an attachment to a note (Priority: P3)

A reader writing a note on the device wants to attach a file kept on the device (a picture, a
scanned page, a PDF). They choose "Add attachment" in the editor, pick the file, confirm, and the
note now shows it in place. They can do this offline; the file goes to the server on the next
sync.

**Why this priority**: It completes the attachment round trip, but it is useful only after
viewing attachments works, and it is the part with the most data-safety risk (uploads, partial
failures), so it comes last.

**Independent Test**: With Wi-Fi off, add an image and a PDF to a note and save. Restart the app,
confirm both are still attached and the note is marked as pending. Turn Wi-Fi on, sync, and verify
both files and the note text are on the server and visible on another device.

**Acceptance Scenarios**:

1. **Given** the user is editing a note, **When** they choose "Add attachment", **Then** they can
   browse the device's storage, select a file, see its name and size, and confirm or cancel.
2. **Given** the user confirmed, **When** they return to the note, **Then** a reference to the file
   appears in the text at the cursor position and, after saving, the reader shows the image in
   place (or the file entry for other types).
3. **Given** the device has no network, **When** the user adds an attachment and saves, **Then**
   the attachment is kept on the device, the note is marked pending, and both survive an app
   restart.
4. **Given** a note has a pending attachment, **When** a sync runs, **Then** the file is uploaded
   first and the note text afterwards, and other devices see both complete after their next sync.
5. **Given** an upload is interrupted, **When** the next sync runs, **Then** it resumes without
   losing the file or the text, and no half-uploaded file is referenced by the server's version
   of the note.
6. **Given** the chosen file is larger than the size limit, **When** the user selects it, **Then**
   it is refused with a clear message and the note is unchanged.
7. **Given** the user cancels the dialog or leaves the editor without saving, **When** they are
   asked to save or discard, **Then** discarding also discards the not-yet-saved attachment, while
   the original file on the device is untouched.

---

### Edge Cases

- A link contains very long or unusual characters (query strings, non-ASCII characters, trailing
  punctuation): the full destination is shown as written when confirmation is needed and passed to
  the browser unchanged, and trailing sentence punctuation such as a closing period or
  parenthesis is not made part of a bare address.
- A link's text looks like one address but its destination is another (for example a title that
  reads as a web address of a different site): the texts differ, so the real destination is shown
  for confirmation.
- A link target looks like an internal note reference or a local file: out of scope; it is shown as
  plain text and is not followed.
- A table has uneven rows (some rows with fewer or more cells than the header): missing cells are
  shown empty and extra cells beyond the header are kept visible, so no content is hidden.
- A table cell contains a literal pipe character written as an escaped pipe: it is displayed as a
  pipe, not as a column separator.
- A table is very large (hundreds of rows): the note remains usable and the app does not become
  unresponsive.
- A table sits inside a list item or a quoted block: shown as plain text rather than a grid.
- A note contains emojis that did not exist when the device's fonts were created: the stand-in
  symbol is shown and the original text is preserved on edit and sync.
- A note contains emojis near the end of a title that is shortened in the list: the shortening
  never splits a symbol in half.
- An image attachment is very large or has an unusual format: it is shown scaled down, or as a
  placeholder if it cannot be decoded, without slowing down the reader beyond the time limits
  below.
- Two attachments have the same file name in different notes: both are kept distinct.
- A note references an attachment that does not exist on the server: a placeholder is shown; no
  error stops the sync. The problem is reported when the user tries to download it.
- The size of an attachment is not known (the device is offline and has not seen it yet, or the
  server does not report it): the placeholder shows the name without a size; the size appears once
  it is learned (opening the note with a connection, or downloading).
- The user declines the confirmation, taps back from the browser right away, or the browser is not
  available: the app stays on the note, and an unavailable browser produces a clear message.
- The user adds the same file twice, or two files with the same name to one note: both are kept
  distinct and neither replaces the other.
- The chosen file disappears (card removed) or cannot be read while being added: the addition
  fails visibly, the note text is unchanged, and no reference is inserted.
- The user deletes the attachment's reference from the note text: the reader no longer shows it;
  removing the file itself from the server is not part of this feature.
- The server rejects an upload (file too large for the server, quota exceeded): the note keeps the
  attachment as pending with the reason shown, the note text is not lost, and other notes sync
  normally.
- The note is deleted on the server while an attachment upload is pending: the MVP deleted-versus-
  edited rule applies and the edited note, with its attachments, is kept.
- Links, tables, and attachments in the same note: each is rendered according to its own rules
  without breaking the others. Inside a table cell, an attachment is shown by its file name only
  (images are drawn only in the body of the note, not inside cells).

## Requirements *(mandatory)*

### Functional Requirements

**Hyperlinks**

- **FR-001**: The reader MUST display web links (written with a title, or as a bare address) as
  visibly distinguishable, tappable links, and MUST show a titled link's title as its text.
- **FR-002**: When the user taps a link whose displayed text is the address itself, the system
  MUST open it immediately without confirmation. The text counts as the address itself if it is a
  web address that names the same destination, ignoring the scheme, a leading `www.`, letter case
  of the host, and a trailing `/`. For any other link, the system MUST show the complete real
  destination address and ask for confirmation before leaving the app; declining MUST leave the
  note and position unchanged.
- **FR-003**: The system MUST open the destination (its real address, not its displayed title) in
  the device's default web browser; on return, the note MUST be shown unchanged at the same
  reading position.
- **FR-004**: The system MUST follow only web links (`http` and `https`). Links of any other type
  MUST be displayed as plain text and MUST NOT be followed.
- **FR-005**: The system MUST NOT contact a link's destination itself (no previews, no
  background checks); only the browser the user chose to open does.
- **FR-006**: If the destination cannot be opened (no browser, no network), the system MUST tell
  the user in plain language and MUST remain on the note.

**Tables**

- **FR-007**: The reader MUST display well-formed Markdown tables (a header row followed by a
  separator row and body rows) as a grid, with the header row visibly distinguished and rows or
  cells separated so the structure is legible in black and white.
- **FR-008**: The reader MUST apply the column alignment (left, center, right) given in the
  separator row.
- **FR-009**: The reader MUST fit every table to the screen width by distributing column widths and
  wrapping cell text; it MUST NOT require sideways scrolling to read any cell.
- **FR-010**: Cells MUST display emphasis, links, and emojis in the same way as normal text, and
  links in cells MUST behave as in FR-002 to FR-004.
- **FR-011**: Text that is not a well-formed table MUST be displayed as ordinary text.
- **FR-012**: When paging through a note, the reader MUST NOT split a table row across two pages.
- **FR-013**: The editor MUST treat tables as ordinary plain text: opening, editing and saving a
  note MUST NOT alter the table text apart from the user's own edits.

**Emojis**

- **FR-014**: The system MUST display emojis in the reader, the note list, titles, and the editor
  wherever text is shown, as monochrome glyphs where the device has them.
- **FR-015**: For emojis the device cannot draw, the system MUST show a clear stand-in symbol and
  MUST keep the original characters in the note unchanged.
- **FR-016**: Multi-part emojis (flags, skin-tone variants, combined sequences) MUST be handled as
  one symbol in display, line wrapping, shortening of titles, and cursor movement.
- **FR-017**: The system MUST store, edit and synchronize note text containing emojis without
  changing, dropping, or replacing any character the user did not edit, in both directions of
  sync.
- **FR-018**: Search MUST find notes by emoji in the same way as by other text.

**Attachments**

- **FR-019**: The reader MUST show every attachment referenced by a note at its place in the text.
  A downloaded image MUST be displayed there, scaled to the available width and rendered legibly
  in grayscale. An attachment that is not downloaded MUST be shown as a placeholder with its file
  name, its size when known, and a Download action.
- **FR-020**: For downloaded attachments that are not images, the reader MUST show an entry with
  the file name and size, and MUST let the user open it with a matching viewer on the device when
  one exists, or tell the user that none exists.
- **FR-021**: The system MUST download an attachment only when the user asks for it (Download
  action); sync MUST NOT download attachments. A downloaded attachment MUST be kept on the device
  so it is available with no network. Downloads MUST follow the same verified connection rules as
  all other server communication.
- **FR-022**: A failed, refused, or missing attachment MUST be shown with its name and the reason,
  MUST NOT prevent the note's text from being read, and MUST NOT affect sync. A download problem
  MUST be reported to the user at the moment they ask for the download.
- **FR-023**: The system MUST remove a downloaded attachment from the device when its note is
  deleted, when the reference to it disappears from the note's text on the server, or when the
  server reports that the file no longer exists, unless the note has unsynced local changes. A
  file deleted on the server while the note text still references it is only noticed when the
  user tries to download it again.
- **FR-024**: Editing and syncing a note MUST NOT remove or change the parts of its text that
  reference attachments, unless the user edits them.
- **FR-025**: In the editor, users MUST be able to add an attachment to a note by choosing a file
  from the device's storage. The system MUST insert the reference to it into the note text at the
  cursor position (so an image appears in place in the reader), and MUST show the file name and
  size before the user confirms the addition.
- **FR-026**: If storage runs out while an attachment is being saved (downloaded or added), the
  system MUST stop, show a visible message, and keep all note text and previously saved
  attachments.
- **FR-027**: An added attachment MUST be stored on the device immediately and MUST be queued for
  upload like any other pending change; it MUST work with no network. A note with an attachment
  that has not yet been uploaded MUST be marked as pending.
- **FR-028**: The system MUST upload an attachment before (or together with) the note text that
  references it, so that no other device ever sees a reference to a file that does not exist on the
  server. An interrupted upload MUST NOT lose the attachment or the note text, and the next sync
  MUST resume it.
- **FR-029**: Adding an attachment MUST only copy the chosen file; the original file on the device
  MUST NOT be moved, changed or deleted. Files over the size limit MUST be refused with a clear
  message before anything is changed in the note.
- **FR-030**: If a note with a pending attachment is in conflict (MVP conflict rules), the conflict
  copy MUST keep its attachments, and both versions MUST remain complete.

- **FR-031**: Tapping a downloaded image MUST open it full screen, fitted to the whole screen in the
  current orientation, with a control that returns to the note at the same reading position. Zoom
  and panning are not provided.

**Safety and privacy**

- **FR-032**: Adding these capabilities MUST NOT weaken the existing rules for data safety:
  nothing in links, tables, emojis or attachments may cause note text to be altered, lost, or
  duplicated by display or by sync.
- **FR-033**: Logs MUST NOT contain link destinations, attachment names, or note content.

**Change to the MVP scope**

- **FR-034**: This feature supersedes the MVP statements that links appear as plain text, that
  tables display as plain text, and that attachments are out of scope (MVP FR-012 and FR-037
  and its assumptions about Markdown and attachments). All other MVP out-of-scope items (widgets,
  multiple accounts, app lock, sharing, dark mode, direct editing, internal note links,
  background sync) stay out of scope.

### Key Entities

- **Link**: A web address in a note, either with a displayed title or written bare. Has a title
  (possibly the address itself) and a destination. Exists only inside the note text.
- **Table**: A block of note text made of a header row, column alignments, and body rows, each
  holding cells. Exists only inside the note text; it is a view of the text, not separate data.
- **Attachment**: A file attached to a note and referenced from its text. Has a file name, a size,
  a type (image or other), the note it belongs to, and a transfer state (not downloaded, downloaded,
  waiting to be uploaded, failed with a reason, removed).
- **Emoji**: A pictographic character or character sequence in note text. Has no storage of its
  own; what matters is that it is shown as one symbol and preserved exactly.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can open a link whose text is its own address in the browser with one tap, and
  any other link with two taps (tap, confirm), and get back to the same note and position with one
  action.
- **SC-002**: In a test set of at least 10 realistic tables (2 to 6 columns, up to 30 rows, mixed
  alignments, with formatted cells), 100% are fully readable on the screen with no sideways
  scrolling and with no row split across pages.
- **SC-003**: In a test set of at least 50 emojis covering single symbols, flags, skin-tone
  variants, and combined sequences, 100% are shown either as a recognizable glyph or as a
  stand-in symbol, and 100% of notes containing them are identical (character for character)
  before and after an edit-and-sync round trip of unrelated text.
- **SC-004**: 100% of attachments the user has downloaded are viewable or openable with the network
  switched off, and a sync of a collection with attachments transfers none of them.
- **SC-005**: A note with a typical mix (a few links, one table, a few emojis, one image of
  about 1 MB) opens and is fully displayed within 3 seconds, in a collection of 1,000 notes
  (the same limit as the MVP).
- **SC-006**: In every tested failure case (attachment missing, download failed, storage full,
  link destination unreachable), 0 notes lose or alter text and the user is told what happened.
- **SC-007**: In a usability pass, at least 9 out of 10 testers open a link, read a table, find an
  attachment, and add an attachment on their first attempt without help.
- **SC-008**: Across a test set of add-attachment and upload interruption scenarios (connection
  drop, app closed, server rejection, storage full), 0 attachments or notes are lost, 0 server
  notes reference a file that was not uploaded, and 0 original files on the device are changed.
- **SC-009**: A user can add an existing file to a note and save it in under 1 minute, with no
  network.

## Assumptions

- "Default web browser" is the browser that ships with or is set up on the PocketBook device; the
  app hands the address to the device and does not provide its own browser.
- Confirmation is required only when a link's text differs from its destination (decided in
  clarification). This keeps plain addresses at one tap while exposing the real address behind any
  title, which also protects against misleading titles. Most descriptive titled links therefore
  ask for confirmation, since their text is not the address. An accidental tap on a plain address
  still leaves the app, an accepted trade-off.
- Only `http` and `https` links are followed. Email links, phone numbers, file paths, and other
  schemes are shown as plain text. Internal note-to-note links remain out of scope (as in the
  MVP).
- Table support covers the common Markdown pipe-table form (header row, separator row with
  optional alignment markers, body rows), as written by the Notes web app and mobile apps. Other
  table dialects display as plain text.
- Tables are for reading. Editing remains plain text; there is no table editor.
- Emojis are shown in black and white, because the screen is monochrome and the device's fonts may
  contain only some of them. Exact appearance may differ from other devices; preservation of the
  characters matters more than their appearance.
- Entering emojis depends on what the on-screen keyboard offers. An emoji picker inside the app is
  out of scope for this feature.
- Attachments follow the convention of the Notes service: files stored with a note on the server
  and referenced from its text. The app does not interpret other file locations.
- Image attachments in the common web formats (such as JPEG, PNG, GIF) are supported; others are
  treated as files that can be opened with a matching device viewer, or shown as placeholders.
- A per-file size limit applies to downloads and uploads so a single large file cannot fill the
  device or be refused by the server late; a download over the limit is refused with a message,
  and an addition over the limit is refused up front. The exact limit is a planning decision.
- Adding an attachment means choosing an existing file on the device; the app does not take photos
  or scan documents itself. Choosing is done inside the app by browsing the device's storage.
- Removing or renaming an attachment file on the server from the device is out of scope; only
  adding is provided. Deleting the reference from the note text just stops showing it.
- Because attachments are downloaded only on request, they do not add to sync time or storage
  until the user chooses; the MVP sync goals apply unchanged. Uploads of attachments the user added
  do happen during sync.
- Existing rules still apply unchanged: one account, certificates always verified, offline use,
  explicit save, no silent data loss, and the conflict rules of the MVP.
