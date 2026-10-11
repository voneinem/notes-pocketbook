# Feature Specification: Explicit Note Title and Category Picker

**Feature Branch**: `feature/004-title-category-picker`

**Created**: 2026-10-11

**Status**: Draft

**Input**: User description: "Explicit Title edit - no guess from first line, Kategory Dropdown with the option to select a previous entered category - option to define a new one"

## Clarifications

### Session 2026-10-11

- Q: Should the app refuse to save a note that has text but no title, or save it with a placeholder? → A: Require a title; a note with an empty title cannot be saved (FR-004).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Give a note an explicit title (Priority: P1)

A reader writes or edits a note and sees a separate "Title" field above the text. They type the title
they want, independent of what the text says, and save. From then on the note is listed and read
under exactly that title. The app no longer takes the first line of the text as the title: changing
or deleting the first line of the text does not change the title, and changing the title does not
touch the text.

**Why this priority**: Today the title is a guess from the first line (or the server's guess), so a
reader cannot name a note without rewriting its text. An explicit title is the core of this feature
and is valuable even without the category picker.

**Independent Test**: Create a note with title "Groceries" and text that starts with a different
line; save and verify the list and reader show "Groceries". Edit the first line of the text, save,
and verify the title is still "Groceries". Change only the title, save, and verify the text is
unchanged.

**Acceptance Scenarios**:

1. **Given** the reader opens a new note, **When** the editor is shown, **Then** it has an empty
   Title field separate from the text, and the on-screen keyboard can be used to fill it.
2. **Given** a note with title "Groceries", **When** the reader changes the first line of its text
   and saves, **Then** the note is still shown as "Groceries" in the list and in the reader.
3. **Given** an existing note, **When** the reader opens it in the editor, **Then** the Title field
   already contains the title the note is currently shown with, and the reader can change it.
4. **Given** the reader changed only the title, **When** they save, **Then** the new title is shown
   in the list and the reader, and the text is unchanged.
5. **Given** a new note whose text is filled in but whose Title field is empty (or only spaces),
   **When** the reader tries to save, **Then** the note is not saved, a clear message says a title
   is needed, and the reader stays in the editor with their text kept.
6. **Given** the reader leaves the editor with an unsaved title change, **When** they leave,
   **Then** they are asked whether to save or discard, exactly as for unsaved text changes.
7. **Given** a title was changed on the reader while offline, **When** the next sync runs, **Then**
   the new title is sent to the server together with the note, and the server's other apps show it.
8. **Given** a title was changed on another device or the server, **When** the next sync runs and
   the note has no unsaved local changes, **Then** the new title is shown on the reader.

---

### User Story 2 - Pick a category from a list (Priority: P1)

In the editor, the Category control shows the note's current category (or "No category"). When the
reader taps it, a list opens with "No category", every category already in use by any note on the
reader, and a "New category…" entry. Choosing an existing category assigns it with one tap; no typing
is needed. The list closes and the control shows the choice.

**Why this priority**: Typing a category on an e-ink on-screen keyboard is slow and error-prone, and
a single typo creates a second, near-duplicate category. Picking from known categories removes both.

**Independent Test**: With notes in categories "Work" and "Home", open another note's editor, tap
the Category control, verify both names plus "No category" and "New category…" are listed, choose
"Home", save, and verify the note appears under the Home view with no typing involved.

**Acceptance Scenarios**:

1. **Given** notes exist in categories "Home" and "Work", **When** the reader taps the Category
   control in the editor, **Then** a list shows "No category", "Home", "Work" and "New category…",
   and marks the note's current choice.
2. **Given** the list is open, **When** the reader chooses "Work", **Then** the list closes, the
   control shows "Work", and saving the note assigns it to the Work category.
3. **Given** a note with category "Work", **When** the reader chooses "No category", **Then** the
   control shows "No category" and saving makes the note uncategorized.
4. **Given** the list is open, **When** the reader dismisses it without choosing, **Then** the note's
   category is unchanged.
5. **Given** there are more categories than fit on one screen, **When** the list is open, **Then**
   the reader can move through it page by page with the device's page buttons or the on-screen
   page controls and reach every category.
6. **Given** no note has a category yet, **When** the reader opens the list, **Then** it shows only
   "No category" and "New category…".
7. **Given** a category exists only on the reader because a note was created or changed offline,
   **When** the list is opened, **Then** that category is listed too.

---

### User Story 3 - Define a new category from the picker (Priority: P2)

A reader wants to file a note under a category that does not exist yet. In the category list they
choose "New category…", type a name with the on-screen keyboard, and confirm. The new name becomes
the note's category and, once the note is saved, is offered in the list for all other notes.

**Why this priority**: Without this, the picker could only reuse existing categories and a reader
could never start a new one. It is lower than the picker itself because categories are created far
less often than they are reused.

**Independent Test**: Open a note's editor, tap Category, choose "New category…", type "Travel",
confirm, save, then open a different note and verify "Travel" is now in its category list.

**Acceptance Scenarios**:

1. **Given** the category list is open, **When** the reader chooses "New category…", **Then** a
   text entry for the name is shown with the on-screen keyboard, with Confirm and Cancel.
2. **Given** the reader typed "Travel" and confirms, **When** they return to the editor, **Then** the
   control shows "Travel", and saving the note assigns it to that new category.
3. **Given** a new category "Travel" was saved on a note, **When** the reader opens the category
   list of any other note, **Then** "Travel" is listed.
4. **Given** the reader typed a name that exactly matches an existing category, **When** they
   confirm, **Then** the existing category is selected and no duplicate is created.
5. **Given** the reader typed an invalid name (empty or only spaces, longer than 255 characters, or
   containing control characters), **When** they confirm, **Then** a clear message explains the
   problem, nothing is assigned, and the reader can correct the name or cancel.
6. **Given** the reader cancels the new-category entry, **When** they return, **Then** the note's
   previous category is unchanged.
7. **Given** the reader created a new category in the picker but leaves the editor without saving
   the note, **When** they open any category list afterwards, **Then** that unsaved category is
   not listed.

---

### Edge Cases

- A note saved before this feature, or synced from the server, has a title that was derived from its
  text: the reader sees that same title in the Title field and keeps it until they change it; the
  app does not rewrite it on its own.
- The title is changed to one that another note already has: both are kept as distinct notes; the
  save is not refused or merged.
- The title is very long (over 255 characters), has leading or trailing spaces, or contains line
  breaks or other control characters: leading/trailing spaces are trimmed; line breaks or control
  characters or excess length are refused with a clear message and the previous title is kept.
- The server adjusts a title it cannot store as typed (for example characters not allowed in file
  names): after the next sync the note shows the title the server reports, and no text is lost.
- Both the title and the text of one note were changed on the reader and on another device before
  sync: this is an ordinary conflict, and both versions are kept as before; the title never causes
  one side to silently win.
- A note with unsaved local changes from before this feature is opened: its Title field shows the
  title it was listed with, not a fresh guess from the first line.
- The category list is long (dozens of categories): the list stays usable by paging and the
  reader can still reach "No category" and "New category…" without paging through everything.
- Two categories differ only by letter case (for example "work" and "Work"): they are different
  categories and both are listed; the app does not merge them.
- A category holds a `/`: it is listed and assigned as typed, as a single flat name.
- The last note of a category is moved to another category: that category disappears from the list.

## Requirements *(mandatory)*

### Functional Requirements

**Title**

- **FR-001**: The note editor MUST show a Title field, separate from the note text, that the
  reader can fill and change with the on-screen keyboard.
- **FR-002**: The system MUST treat the title the reader entered as the note's title and MUST NOT
  derive, replace or update a title from the first line of the text. Editing the text MUST NOT
  change the title, and editing the title MUST NOT change the text.
- **FR-003**: When an existing note is opened in the editor, the Title field MUST contain the title
  the note is currently shown with.
- **FR-004**: A note MUST NOT be saved with an empty or whitespace-only title. The system MUST
  explain this in a clear message, keep the reader in the editor, and keep everything they typed.
  A new note with neither title nor text MUST still not be stored (the existing rule for empty
  notes stays in force).
- **FR-005**: The system MUST trim leading and trailing spaces of a title, and MUST refuse a title
  that is longer than 255 characters or contains line breaks or other control characters, with a
  clear message, keeping the previous title.
- **FR-006**: A changed title MUST count as an unsaved change: leaving the editor with it MUST ask
  the reader whether to save or discard, as for the text.
- **FR-007**: Titles MUST be listed, searched, sorted by and read exactly as entered. For a note in
  sync with the server the title shown MUST be the one the server reports, which equals the entered
  title unless the server adjusted it.
- **FR-008**: A title change MUST work with no network connection, be queued for the next sync, and
  be sent to the server with the note. The system MUST NOT send a title that was guessed from the
  text.
- **FR-009**: A title change made elsewhere MUST appear on the reader at the next sync when the note
  has no unsaved local changes; when the note has local changes, the existing conflict rules apply
  and neither version is discarded silently.
- **FR-010**: Two notes with the same title MUST be allowed and kept distinct.
- **FR-023**: The short preview shown under a note's title in the list MUST NOT drop any text the
  reader wrote: it starts at the first line of the text, except that a first line which only
  repeats the title (ignoring a leading heading marker) is left out, as it is for notes written
  before this feature.

**Category picker**

- **FR-011**: The editor MUST present the category as a picker control showing the current choice
  or "No category", instead of a free-text field.
- **FR-012**: Opening the picker MUST show a list containing "No category", every category
  currently used by at least one note on the reader (including notes not yet synced), and a "New
  category…" entry, with the current choice marked. Categories MUST be listed in alphabetical
  order, without duplicates.
- **FR-013**: Choosing an existing category or "No category" MUST set it as the note's category in
  the editor with a single selection and no typing, and close the list. Dismissing the list without
  a choice MUST leave the category unchanged.
- **FR-014**: Choosing "New category…" MUST let the reader type a category name and confirm or
  cancel. A confirmed valid name MUST become the note's category in the editor.
- **FR-015**: A typed category name MUST be accepted only if it is non-empty, at most 255
  characters, has no leading or trailing spaces (trimmed away) and no control characters;
  otherwise the system MUST show a clear message and assign nothing. A name containing `/` MUST be
  accepted and treated as one flat name.
- **FR-016**: A typed name that exactly matches an existing category MUST select that category
  instead of creating a second one. Names that differ in letter case MUST be treated as different
  categories.
- **FR-017**: A category chosen or created in the picker MUST take effect only when the note is
  saved. A new category MUST appear in the picker of every other note once a note carrying it is
  saved, and MUST NOT appear if the editor is left without saving.
- **FR-018**: A category with no remaining notes MUST disappear from the picker and from the note
  list views, as today.
- **FR-019**: The picker list MUST be usable with long category lists by moving page by page with
  the device's page buttons or on-screen page controls, and MUST keep "No category" and "New
  category…" reachable without paging to the end.
- **FR-020**: The picker and the title field MUST work with no network connection, and category
  and title changes MUST be queued for the next sync like other note changes.

**Compatibility**

- **FR-021**: Existing notes MUST keep the title and category they have; opening this version MUST
  NOT change, rename or recategorize any note by itself.
- **FR-022**: This feature replaces the earlier rule that a note's title is the first line of its
  text and that the editor has no title field (feature 001, FR-019 and its related clarification
  and assumption). All other behaviour of notes, categories, sync and conflicts stays as specified
  there.

### Key Entities *(include if feature involves data)*

- **Note**: A piece of text with an explicit title chosen by the reader, a category (possibly
  none), a favorite marker and a last-modified time. The title is stored and synced independently of
  the text.
- **Title**: A single line of 1–255 characters entered by the reader. Not unique. Never derived from
  the text.
- **Category**: A label shared by notes, derived from the notes themselves; a category exists as long
  as at least one note uses it. A note has at most one. The picker lists the categories that exist.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A reader can set or change a note's title without touching its text, and in 100% of
  test cases the title shown in the list and reader equals what they entered (barring server
  adjustment) after any edit of the text.
- **SC-002**: A reader can assign an existing category to a note in at most 3 taps (open the
  category control, choose the category, plus saving as usual) and zero typed characters.
- **SC-003**: A reader can create and assign a new category in under 30 seconds on the device,
  and that category is offered in the picker of the next note they open in 100% of test cases.
- **SC-004**: After this feature, no note's title changes as a side effect of editing its text,
  and no category is created by a typo that the picker could have prevented: choosing from the list
  never creates a second spelling of an existing category.
- **SC-005**: Every title and category change made offline is delivered by the next sync with no
  note lost or overwritten without a conflict copy, matching the existing guarantees.
- **SC-006**: A reader with 50 categories can reach any category in the picker using page controls
  alone, with "No category" and "New category…" visible on the first page.

## Assumptions

- The title is required (confirmed in clarification): a note without a title cannot be saved. This
  keeps the rule "no guessing" consistent, because the server would otherwise derive its own title
  from the text. Existing and synced notes always have a title, so only newly written notes are
  affected.
- The picker lists exactly the categories derived from the notes on the reader, as the existing
  views do; there is no separate category store, rename or delete operation (still out of scope).
- A category is one flat name; no folder-style hierarchy is shown, even if the name has a `/`.
- The title and category limits and character rules match those already applied to category names
  (255 characters, no control characters, trimmed spaces); a title may additionally not contain
  line breaks.
- The pick list uses the same touch, page-button and e-ink conventions as the app's other lists
  and dialogs; the design of the control is decided in planning.
- Notes with local changes made before this feature keep the title they were listed with and become
  explicit titles once saved; no migration prompts the reader.
- The server accepts an explicit title with a note and reports back the title it stored; a server
  that cannot store an explicit title shows its own derived title after sync, which the app then
  displays as for any synced note.
- Out of scope: renaming or deleting a category across notes, category hierarchy or folders, sorting
  or filtering changes in the list, and bulk changes to titles or categories.
