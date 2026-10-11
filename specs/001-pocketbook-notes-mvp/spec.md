# Feature Specification: PocketBook Notes MVP

**Feature Branch**: `001-pocketbook-notes-mvp`

**Created**: 2026-10-10

**Status**: Draft

**Input**: User description: "A notes app for the PocketBook InkPad One that syncs with a Nextcloud Notes server. Reference: docs/notes-android-features.md (section 10, "Port first"). MVP scope: setup (server URL, username, app password, connection test, certificates verified); note list with views, sorting and substring search; reader with basic Markdown, toggleable task-list checkboxes and adjustable font size; plain-text editor with explicit save, create, delete with confirmation, category and favorite, all working offline and queued for sync; manual and start-time sync with visible status, pushing local changes before pulling remote ones; conflicts never overwrite silently and keep both versions; all synced notes readable and editable offline. Out of scope: widgets, multiple accounts, app lock, sharing, dark mode, attachments, direct edit, internal note links, background sync."

## Clarifications

### Session 2026-10-10

- Q: When a note was edited on both the device and the server, which version stays the original
  and which becomes the conflict copy? → A: The server's version stays as the original note; the
  device's version becomes the conflict copy.
- Q: Should this release allow trusting a self-signed certificate through an explicit, warned
  option? → A: No. Not in the MVP; connections to servers with unverifiable certificates are
  always refused.
- Q: Is a note's title the first line of its text, or a separate title field? → A: The first
  non-empty line of the text; there is no separate title field. (**Superseded by feature 004:** the
  title is a separate field the reader enters; it is never derived from the text.)
- Q: Is a conflict copy uploaded to the server like any other new note? → A: Yes. It is uploaded
  and syncs like any other note.
- Q: May users extend the trusted certificate authorities with their own CA file (for a private
  CA or a self-signed server certificate)? → A: Yes. A user-supplied CA file is added to the
  trusted authorities; verification always stays on, there is no "trust anything" option, and
  Settings shows whether a custom CA is loaded. (This refines the self-signed answer above: there
  is still no in-app option to accept an unverifiable certificate.)
- Q: How are the displayed title, category characters, identical edits on both sides, and a lost
  create confirmation handled? → A: (decided 2026-10-10) The displayed title is the server's title for notes in sync and the first line of the text for notes with local changes (changed 2026-10-10 after device testing: titles differ from the server's once notes are renamed or cleaned up there); a category may contain `/` (shown flat) but not control characters,
  leading or trailing spaces, or more than 255 characters; identical text on both sides creates no
  conflict copy; a note can be duplicated only if the server accepted a new note but the
  confirmation never reached the device (nothing is lost).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Connect to my notes server (Priority: P1)

A reader opens the app for the first time and enters the address of their Nextcloud server, their
username, and an app password. They run a connection test, see whether it succeeded, and, once it
has, the app is set up and ready to download their notes. The app only accepts servers whose
security certificate it can verify.

**Why this priority**: Without a working, trusted connection nothing else (sync, notes) is
possible, and credentials must never be sent over an unverified connection.

**Independent Test**: Enter valid server details and run the connection test. The app reports
success and saves the setup. Repeat with a wrong password and with an unreachable address. Each
failure gets a distinct, understandable message and nothing is saved.

**Acceptance Scenarios**:

1. **Given** the app has not been set up, **When** the user enters a valid server address, username
   and app password and runs the connection test, **Then** the app shows that the connection works
   and lets the user finish setup.
2. **Given** the user enters a wrong username or app password, **When** the connection test runs,
   **Then** the app reports that the credentials were rejected and does not save the setup.
3. **Given** the server's certificate cannot be verified, **When** the connection test runs,
   **Then** the app refuses the connection, explains that the certificate could not be verified,
   and does not send the credentials.
4. **Given** the server is unreachable (no Wi-Fi, wrong address, server down), **When** the
   connection test runs, **Then** the app reports that the server could not be reached and
   distinguishes this from rejected credentials.
5. **Given** the server does not offer a compatible notes service, **When** the connection test
   runs, **Then** the app tells the user that the server cannot be used with this app.
6. **Given** the server's certificate was issued by a private authority, **When** the user has
   placed that authority's certificate file in the app's data folder and runs the connection test,
   **Then** the connection succeeds with verification still on; **When** the file is missing or
   invalid, **Then** the test fails as in scenario 3.

---

### User Story 2 - Read my notes, also offline (Priority: P1)

After setup, the reader sees their notes downloaded from the server, opens a note in a large,
readable view, and can keep reading and browsing after disconnecting from Wi-Fi.

**Why this priority**: Reading synced notes on an e-ink device is the core value and the smallest
useful slice of the product.

**Independent Test**: Set up the app against a server holding sample notes, run a sync, switch
off Wi-Fi, and open several notes from the list. All of them display fully.

**Acceptance Scenarios**:

1. **Given** setup is complete and the server holds notes, **When** the first sync finishes,
   **Then** every server note appears in the note list with its title, category, modification date
   and favorite marker.
2. **Given** notes have been synced, **When** the device has no network, **Then** the user can still
   browse the list and open and read every synced note.
3. **Given** a note contains headings, emphasis, bullet or numbered lists, links, and task-list
   items, **When** the user opens it, **Then** headings and emphasis are visibly distinguished,
   lists are laid out as lists, links appear as plain readable text (not followable), and
   task-list items appear as checked or unchecked boxes.
4. **Given** the user is reading a note, **When** they choose small, medium, or large font size,
   **Then** the text re-renders at that size and the choice is kept for later notes and later
   app starts.

---

### User Story 3 - Write and edit notes offline (Priority: P2)

The reader creates a new note, edits an existing one, sets its category, marks it as a favorite,
or deletes it, all with the on-screen keyboard and without needing a network. Changes are saved
only when the user chooses to save and are kept until they can be sent to the server.

**Why this priority**: Capturing and fixing notes completes the read/write loop, but is useful only
once reading and connecting work.

**Independent Test**: With Wi-Fi off, create a note, edit another, change a category, toggle a
favorite, and delete a third (confirming the prompt). Restart the app and confirm all changes
are still present and shown as waiting to sync.

**Acceptance Scenarios**:

1. **Given** the user is in the editor, **When** they type text and press save, **Then** the note is
   stored on the device and shown in the list with a fresh modification date, even with no network.
2. **Given** the user has typed text, **When** they leave the editor without saving, **Then** the app
   asks whether to save or discard; nothing is saved automatically.
3. **Given** the user chooses delete, **When** the confirmation appears and they confirm, **Then**
   the note disappears from the list; if they decline, the note is unchanged.
4. **Given** the user sets a category or toggles the favorite marker on a note, **When** they save
   or toggle, **Then** the change is visible immediately and works without network.
5. **Given** the user created or changed notes offline, **When** the app is closed and reopened
   before any sync, **Then** all those changes are still present and marked as not yet synced.

---

### User Story 4 - Keep notes in sync with the server (Priority: P2)

The reader taps "Sync now" (or simply starts the app while on Wi-Fi) and their local changes are
sent to the server first, then changes made elsewhere are downloaded. The app always shows when
the last sync happened and whether it succeeded.

**Why this priority**: Sync is what makes the app a client for the server rather than an isolated
notebook, but it depends on setup, reading, and local changes already existing.

**Independent Test**: Make changes on the device and on the server, tap "Sync now", and verify
that both sides end up with the same notes and the status line shows the time and "success".

**Acceptance Scenarios**:

1. **Given** the user made changes offline, **When** they tap "Sync now" with a working connection,
   **Then** the new, edited, and deleted notes are applied on the server before remote changes are
   downloaded, and the status shows the sync time and success.
2. **Given** notes were added, changed, or deleted on the server, **When** a sync runs, **Then**
   the device shows the same notes; notes with unsynced local changes are not removed or replaced
   by the download.
3. **Given** Wi-Fi is available, **When** the app starts, **Then** a sync begins automatically; when
   Wi-Fi is not available at start, no sync is attempted and no error is shown.
4. **Given** a sync fails (server unreachable, connection lost, credentials rejected), **When** the
   failure occurs, **Then** the status shows what failed, local data is untouched, and a later sync
   can retry.
5. **Given** a note was created or changed locally and the sync is interrupted midway, **When** the
   next sync runs, **Then** the change is never lost; only if the server had already created the note and the confirmation never reached the device can the note appear twice, with identical text.
6. **Given** the app has never synced or has synced before, **When** the user looks at the status
   area, **Then** it shows either "never synced" or the time and outcome of the last attempt.

---

### User Story 5 - Never lose a note to a conflict (Priority: P2)

When the same note was changed on the server and on the device since they last agreed, the app
does not pick a winner silently. It keeps both versions, clearly labels the extra copy, and tells
the user what happened.

**Why this priority**: Protecting the user's writing is a stated project rule; without it, sync
could destroy data. It only matters once sync exists.

**Independent Test**: Edit the same note on the server and on the device (offline), then sync.
Both texts are present afterwards, one under a clearly named conflict copy, and the user was
told.

**Acceptance Scenarios**:

1. **Given** a note was edited on the device and also changed on the server since the last sync,
   **When** the sync runs, **Then** both versions are kept: the server's version stays as the note
   and the device's version is saved as a separate note whose title clearly marks it as a conflict
   copy of the original; the copy is also uploaded to the server so it appears on other devices.
2. **Given** a conflict was resolved this way, **When** the sync finishes, **Then** the status
   reports that conflicts occurred and how many, and the conflict copies are findable in the list.
3. **Given** a note was deleted on the server but edited on the device, **When** the sync runs,
   **Then** the edited note is kept and sent to the server again instead of being deleted locally.
4. **Given** a note was deleted on the device but edited on the server, **When** the sync runs,
   **Then** the server's edited version is kept and the deletion is not applied silently.
5. **Given** two devices change the same note, **When** either syncs second, **Then** no change
   from either device disappears without the user having been told.

---

### User Story 6 - Organize and find notes (Priority: P3)

The reader narrows the list to All, Favorites, Uncategorized, or one category, sorts it by last
modified or by title, and searches for text in titles and note contents.

**Why this priority**: These make a larger collection usable, but the app is already valuable with
a plain list.

**Independent Test**: With notes in several categories and some favorites, switch between views,
change the sort order, and search a word that appears only in one note's body. The list shows the
expected notes each time.

**Acceptance Scenarios**:

1. **Given** notes in different categories, **When** the user selects a category, **Then** only
   notes of that category are listed; "All" lists everything, "Favorites" only favorites, and
   "Uncategorized" only notes without a category.
2. **Given** a list is shown, **When** the user sorts by modified date, **Then** the most recently
   modified note is first; **When** they sort by title, **Then** notes appear in alphabetical order.
3. **Given** the user enters search text, **When** a note's title or content contains that text
   (ignoring upper/lower case), **Then** the note appears in the results, and others do not.
4. **Given** the user toggles a task-list checkbox while reading, **When** they leave the note,
   **Then** the new state is saved like any edit and queued for sync.
5. **Given** the user searches or filters while offline, **When** they do so, **Then** it works on
   all downloaded notes.

---

### Edge Cases

- The server's certificate expires or changes after setup: sync refuses the connection and says
  why; local notes stay readable and editable.
- The user's app password is revoked on the server: the next sync reports rejected credentials and
  offers to re-enter them; local notes and pending changes are kept.
- Wi-Fi drops during a sync: the sync ends with a clear failure status; pending changes remain
  pending and nothing is duplicated or lost.
- The server is in maintenance mode: the sync reports that the server is temporarily unavailable
  without treating it as an error with the user's data.
- A new note is empty or whitespace-only when the user leaves the editor: it is not stored.
- A note is very long (tens of thousands of characters): it can be opened, paged through, edited
  and saved without the app becoming unresponsive.
- Two notes have the same title, or a title contains characters the server restricts: both are
  kept distinct and sync does not drop or merge either.
- A category name is empty (the note is uncategorized), longer than 255 characters, has leading or
  trailing spaces, or contains control characters: the app rejects it with a clear message and
  keeps the previous category. A category containing `/` is accepted and shown as typed (flat).
- The connection drops after the server created a new note but before the app received the
  confirmation: the next sync creates the note again, so it can appear twice with identical text;
  no text is lost.
- A note was edited identically on the device and on the server: no conflict copy is created.
- The device runs out of storage while saving: the save fails visibly and the previous version
  of the note is preserved.
- The device clock is wrong: modification-date ordering may look odd, but no note is lost or
  overwritten because of it.
- A conflict occurs on a note that already has a conflict copy: a further, distinctly named copy
  is created rather than replacing the earlier one.
- The user taps "Sync now" while a sync is running: the request does not start a second
  overlapping sync and the user sees that one is already in progress.

## Requirements *(mandatory)*

### Functional Requirements

**Setup and connection**

- **FR-001**: Users MUST be able to enter a server address, a username, and an app password and
  run a connection test before setup is completed.
- **FR-002**: The connection test MUST distinguish and report: success, unreachable server,
  rejected credentials, unverifiable certificate, and incompatible server.
- **FR-003**: The system MUST verify the server's certificate on every connection that carries
  credentials and MUST NOT send credentials when verification fails. This release MUST NOT offer
  any option to bypass verification or to accept an unverifiable certificate; trust can only be
  extended with a user-supplied CA file (FR-038).
- **FR-004**: The system MUST store the credentials only on the device, MUST NOT show the
  password after entry, and MUST NOT write it to logs or exported data.
- **FR-005**: The system MUST support exactly one server account in this release.
- **FR-038**: If the user places a certificate-authority file named `custom-ca.pem` in the app's
  data folder, the system MUST add the certificates in that file to the trusted authorities for
  all server connections, in addition to the device's (or the app's bundled) authorities.
  Verification MUST stay enabled in every case. An empty, unreadable, oversized or non-certificate
  file MUST be ignored with a visible warning. The Settings screen MUST show whether a custom CA
  is loaded (none, loaded with the number of certificates, or invalid).

**Note list and organization**

- **FR-006**: The system MUST list notes showing title, category, modification date, and a
  favorite marker.
- **FR-007**: The system MUST offer the views All, Favorites, Uncategorized, and one view per
  existing category, and MUST show which view is active.
- **FR-008**: Users MUST be able to sort the list by last-modified date (newest first) or by title.
- **FR-009**: Users MUST be able to search note titles and contents for a substring,
  case-insensitively, over all downloaded notes without a network connection.
- **FR-010**: Notes whose changes have not yet been sent to the server MUST be marked as pending
  in the list.

**Reading**

- **FR-011**: The system MUST display a note's text in a large, readable view suitable for an
  e-ink screen, paging or scrolling without animation.
- **FR-012**: The reader MUST render headings, emphasis, bullet and numbered lists, and task-list
  items distinctly, and MUST show links as plain text (not followable).
- **FR-013**: Users MUST be able to toggle a task-list checkbox in the reader; the new state MUST
  be saved to the note and queued for sync like any other edit.
- **FR-014**: Users MUST be able to choose a small, medium, or large font size, and the choice
  MUST persist across notes and app restarts.

**Editing**

- **FR-015**: Users MUST be able to create a note, edit a note as plain text with the on-screen
  keyboard, and save explicitly. The system MUST NOT save a note automatically on a timer.
- **FR-016**: When the user leaves the editor with unsaved changes, the system MUST ask whether to
  save or discard them.
- **FR-017**: Users MUST be able to delete a note, and the system MUST ask for confirmation first.
- **FR-018**: Users MUST be able to set or change a note's category and toggle its favorite
  marker, with the change taking effect immediately.
- **FR-019**: A note that has local changes MUST show the first non-empty line of its text as its
  title and MUST update it whenever that line changes. A note that is in sync with the server MUST
  show the title the server reports for it (the title the web and Android apps show); if the server
  reports none, or the note has no text and no server title, the first line (or "(empty note)")
  is shown. The editor has no separate title field and the app never sends a title.
  (**Superseded by feature 004, `specs/004-title-category-picker/spec.md`:** the editor has a Title
  field, the stored title is what the reader entered, and it is sent with every write.)
  (**Extended by feature 005, `specs/005-text-editing-reader-title/spec.md`:** the reading page starts with the
  note's title as a separate title block above the text, and the editor gains select, copy, cut, paste, delete,
  undo and redo.)
- **FR-020**: The system MUST NOT store a new note that is empty or whitespace-only.

**Offline behavior**

- **FR-021**: All note operations (read, search, filter, sort, create, edit, delete, change
  category, toggle favorite, toggle task-list item) MUST work with no network connection.
- **FR-022**: Changes made offline MUST be persisted immediately, MUST survive app restarts, and
  MUST be queued so they are sent on the next successful sync.

**Synchronization**

- **FR-023**: Users MUST be able to start a sync with a "Sync now" control.
- **FR-024**: The system MUST start a sync automatically when the app starts and Wi-Fi is
  available, and MUST NOT report an error if Wi-Fi is unavailable at start.
- **FR-025**: Each sync MUST send local changes (new, edited, deleted notes) to the server first
  and then download remote changes.
- **FR-026**: Downloading MUST NOT overwrite or remove a note that has unsynced local changes.
- **FR-027**: The system MUST show the time and outcome (success, partial, failed, offline, server in
  maintenance, in progress, never synced) of the last sync at all times in the main view.
- **FR-028**: Only one sync MUST run at a time; a second request during a sync MUST NOT start an
  overlapping one.
- **FR-029**: A failed or interrupted sync MUST NOT lose or corrupt any note, and the next sync
  MUST pick up where pending changes remain. A note MUST NOT be duplicated, except when the server
  accepted a new note but its confirmation never reached the device; the duplicate then has
  identical text.
- **FR-030**: Notes removed on the server MUST be removed from the device only if they have no
  unsynced local changes.
- **FR-031**: Background sync is not provided; the system MUST sync only on user request or at
  app start.

**Conflicts and data safety**

- **FR-032**: The system MUST detect when a note was changed on the server since the device last
  saw it, before overwriting the server's version with a local change.
- **FR-033**: On such a conflict the system MUST keep both versions, leaving the server's version
  as the original note and saving the local version as a separate note whose title clearly
  identifies it as a conflict copy of the original (including the conflict date). The conflict
  copy MUST be uploaded to the server as a new note and then sync like any other note. If the
  local and server versions have identical text, category and favorite, no conflict copy is
  created and the note is simply in sync.
- **FR-034**: The system MUST tell the user, in the sync status, how many conflicts occurred, and
  conflict copies MUST be reachable from the note list.
- **FR-035**: A note deleted on one side and edited on the other MUST be kept (the edited version
  survives) and the user MUST be informed.
- **FR-036**: The system MUST NOT discard a local change until the server has confirmed it.

**Out of scope for this release**

- **FR-037**: This release MUST NOT include widgets, multiple accounts, app lock, sharing,
  dark mode, attachments, direct (in-browser) editing, internal note links, or background sync.

### Key Entities

- **Server Connection**: The single configured account: server address, username, app password,
  and the result of the last connection test. Exactly one exists after setup.
- **Note**: A piece of text with a title, a category (possibly none), a favorite marker, a last-
  modified time, its body text, and a sync state (in sync, changed locally, deleted locally,
  conflict copy). It remembers the server's last known version of itself so changes can be
  checked against the server.
- **Category**: A label shared by notes; derived from the notes themselves. A note has at most one
  category; a category exists as long as at least one note uses it.
- **Pending Change**: A new, edited, or deleted note that has not yet been confirmed by the server.
- **Sync Status**: When the last sync ran, how it ended, and how many notes were pushed, pulled,
  and in conflict.
- **Conflict Copy**: A note created by the app to preserve a local version that conflicted with a
  server change; labelled with the original title and the conflict date.
- **Reading Preferences**: The chosen font size and the chosen list view and sort order.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A first-time user can complete setup (enter details, run the connection test, finish)
  in under 3 minutes.
- **SC-002**: After the first sync, 100% of the notes on the server are readable on the device with
  the network switched off.
- **SC-003**: A user can open any synced note from the list and see it fully displayed within
  3 seconds with a collection of 1,000 notes.
- **SC-004**: A user can create a new note, type a short note, and save it in under 1 minute, with
  no network.
- **SC-005**: Across a test set of edit/sync interruption scenarios (connection drop, app closed,
  server error), 0 notes are lost, corrupted or silently overwritten, and a note is duplicated only when a create was accepted by the server but its confirmation was lost (identical text).
- **SC-006**: In every tested conflict scenario (both sides edited, deleted-versus-edited, repeated
  conflicts), both versions of the text remain available to the user and the user is told.
- **SC-007**: 100% of connections that carry credentials are refused when the server certificate
  cannot be verified, with an explanatory message.
- **SC-008**: Search over 1,000 notes returns results in under 2 seconds.
- **SC-009**: Syncing 1,000 notes where only a few changed completes in under 30 seconds on a
  typical home Wi-Fi connection.
- **SC-010**: The last-sync time and outcome are visible without any extra navigation on the note list
  screen (the main view).
- **SC-011**: In a usability pass, at least 9 out of 10 testers can find and use Sync now, switch
  views, and change the font size on their first attempt without help.

## Assumptions

- The target device is the PocketBook InkPad One, used with its touch screen and on-screen
  keyboard; large touch targets and minimal screen redraws are expected.
- The user already has a Nextcloud account with the Notes service enabled and can create an app
  password; the app does not create accounts or app passwords.
- Only one account is supported; moving notes between accounts is out of scope.
- Only a current version of the Notes service is supported; the connection test reports older,
  incompatible servers clearly rather than working around them.
- Certificates must be verifiable using the device's trusted certificate authorities or a
  certificate bundle shipped with the app, optionally extended by a user-supplied CA file
  (FR-038). Users whose server uses a private CA or a self-signed certificate place that
  certificate in the app's data folder; there is no in-app option to accept an unverifiable
  certificate. How users copy the file onto the device is to be confirmed and documented in the
  install guide.
- The note title is normally the first non-empty line of the text, matching how the server names notes; for notes in sync the title the server reports is shown (it can differ after renames);
  there is no separate title field in the editor (confirmed in clarification; **superseded by
  feature 004**, which adds an explicit title field).
- A "conflict" is defined as a note changed on the device and also changed on the server since the
  device last synchronized it. The server's version stays as the original note and the device's
  version becomes the conflict copy (confirmed in clarification), so the server remains the shared
  source of truth.
- A category is a single label per note (the server's model). Hierarchical categories are shown
  flat.
- "Wi-Fi available" means the device is connected to a wireless network; it is not checked whether
  the server is reachable before attempting a sync.
- Task-list checkboxes are toggled in the reader; their state is stored in the note's text so it
  syncs like any other edit.
- The app is used by one person on one device at a time per account; simultaneous use on several
  devices is handled only through the conflict rules above.
- Markdown support is limited to headings, emphasis, lists, links as plain text, and task lists;
  other constructs (tables, code blocks, images) display as plain text.
- Notes are text only; attachments and images are out of scope.
- The reference behavior for this scope is `docs/notes-android-features.md` (section 10, "Port
  first"). Behavior is reimplemented from the public Notes service behavior, not copied from that
  app.
