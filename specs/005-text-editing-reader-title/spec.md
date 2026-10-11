# Feature Specification: Text Editing Tools and Title in the Reader

**Feature Branch**: `feature/005-text-editing-reader-title`

**Created**: 2026-10-11

**Status**: Draft

**Input**: User description: "Select/Copy/Cut/Paste/Delete text in title and note text + Undo and Redo. When a note is not in edit mode also show the title in the details page."

## Clarifications

### Session 2026-10-11

- Change after the first device test (2026-10-11, the author): the row of command buttons was too much and
  selecting did not work. The editor header keeps only Cancel, Save, Favorite, Attach and Delete. A **long press
  on a word** selects it and shows a **bubble menu** with the other commands and **two draggable markers** at the
  start and end of the selection, like the reader's own selection (the author's screenshot). A long press on
  empty text offers the commands that need no selection. The reading page works the same way (bubble with Copy and
  Select all, markers; the footer keeps only the page buttons). The earlier "Extend" button and the editor's tools
  row are gone.
- Q: Should the reader be able to select and copy text while reading a note, or only in the editor? → A: Both: editor (select, Copy, Cut, Paste, Delete, Undo, Redo) plus selecting and Copy on the reading page (no Cut, Paste or Delete there).
- Q: When text is copied from the reading page, is the result the displayed text or the note's source text with its formatting marks? → A: The source text with its formatting marks.
- Q: Should copied text be shared with other apps on the reader, or stay private to this app? → A: Shared with other apps on the reader, where the device supports it.
- Q: Should Undo and Redo also cover category and favorite changes, or only title and text? → A: Everything in the editor: title, text, category and favorite.
- Q: When a note's text begins with a line that repeats its title, should the reading page still show the title block? → A: Yes, always show it; the title can then appear twice.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Select text and copy, cut, paste or delete it (Priority: P1)

A reader editing a note wants to move, duplicate or remove a piece of writing without retyping it on
the e-ink keyboard. In the editor they select some text, in the title or in the note text, and then
copy it, cut it, or delete it. They place the caret somewhere else, in the same field or in the
other one, and paste. A long press on a word selects it and opens a small menu next to the selection with the
commands, and two markers at the ends of the selection can be dragged; no keyboard shortcuts are needed and the
editor's header stays as it is.

**Why this priority**: Retyping on an on-screen keyboard is slow, and today the only way to fix or
move a passage is to delete it character by character. Select and the four commands together are the
core of the feature and are useful even without undo or the reader title.

**Independent Test**: In the editor, type "Hello world" into the text. Select "world", copy it, put
the caret at the start and paste: the text reads "worldHello world". Select "Hello", cut it, then
paste at the end. Select a word and delete it. In every case only the selected characters change.

**Acceptance Scenarios**:

1. **Given** the editor is open, **When** the reader long-presses a word in the text or the title,
   **Then** the word is visibly selected, a marker is shown at each end of the selection and a menu
   appears next to it with Cut, Copy, Remove and Select all (and Paste, Undo, Redo when they apply).
2. **Given** the reader long-presses empty space below the text, **When** the menu appears, **Then** it
   offers only what needs no selection (Paste when something was copied, Select all, Undo, Redo) and
   nothing is selected. A short tap only places the caret and shows no menu.
3. **Given** text is selected, **When** the reader chooses Copy, **Then** the text stays unchanged, the
   selected characters are remembered for pasting, and the menu and markers go away.
4. **Given** text is selected, **When** the reader chooses Cut, **Then** the selected characters are
   removed, the caret is where they were, and they are remembered for pasting.
5. **Given** text is selected, **When** the reader chooses Remove, **Then** the selected characters
   are removed, the caret is where they were, and the remembered (copied) text is not changed.
6. **Given** something was copied or cut, **When** the reader long-presses empty space or a word and
   chooses Paste, **Then** the remembered characters are inserted at the caret (or replace the
   selection) and the caret ends after them.
7. **Given** a selection with its markers, **When** the reader drags the start or the end marker,
   **Then** that end of the selection follows the finger, the menu is hidden while dragging and
   shown again when the finger is lifted.
8. **Given** nothing has been copied or cut yet, **When** the menu opens, **Then** Paste is not
   offered.
9. **Given** the reader selected text in the title field, **When** they copy it and paste it into
   the note text (or the other way round), **Then** it is inserted there; a selection and the
   menu always acts on the field that was pressed.
10. **Given** text with line breaks was copied from the note text, **When** the reader pastes it
    into the title, **Then** the line breaks become single spaces so the title stays one line.
11. **Given** the reader taps elsewhere or types a character, **When** a selection exists, **Then**
    the selection, its markers and the menu go away (typing replaces the selection) as in any text editor.
12. **Given** the reader wants to select everything in a field, **When** they choose Select all in the
    menu, **Then** the whole title or the whole text of the field is selected and the menu stays.

---

### User Story 2 - Undo and redo editing steps (Priority: P1)

A reader makes a mistake while editing, such as cutting the wrong paragraph, pasting into the wrong
place or deleting a selection, and wants to step back. Undo reverses the last editing step; Redo
brings it back. This works for typing as well as for cut, paste and delete, in the title and in the
text, and also for choosing a category and toggling Favorite.

**Why this priority**: Cut, paste and delete are destructive; with a slow keyboard a mistake is
expensive. Undo is what makes them safe to use, so it ships together with them.

**Independent Test**: Type "abc", paste "XY", then Undo: the text reads "abc" again. Redo: it reads
"abcXY". Cut a word, Undo: the word is back at its place. Undo all the way: the note reads as it
did when the editor was opened, and no further Undo is offered.

**Acceptance Scenarios**:

1. **Given** the reader typed text, **When** they tap Undo, **Then** the most recent group of typing
   is removed (not necessarily a single character, but never more than one natural step such as a
   word or a pause in typing) and the caret is where it was.
2. **Given** the reader cut, pasted or deleted a selection, **When** they tap Undo, **Then** exactly
   that action is reversed: the cut or deleted text is back and selected or placed where it was, or
   the pasted text is gone and any text it replaced is back.
3. **Given** the reader undid a step, **When** they tap Redo, **Then** the step is applied again.
4. **Given** the reader undid a step and then types or edits something new, **When** they look at
   Redo, **Then** Redo is unavailable, since the undone steps can no longer be reapplied.
5. **Given** the editor was just opened, **When** the reader looks at Undo and Redo, **Then** both
   are unavailable; the opening content of the note is never undone away.
6. **Given** the reader edited both the title and the text, **When** they tap Undo repeatedly,
   **Then** the steps are undone in the reverse order they were made, across both fields.
7. **Given** the reader undid every step so the note is as it was when opened, **When** they leave
   the editor, **Then** it is no longer asked whether to save or discard, since nothing differs
   from the saved note.
8. **Given** the reader saved the note and stays in or reopens the editor, **When** they look at
   Undo, **Then** steps made before the save are not offered (the history starts at opening).
9. **Given** the reader chose a different category (an existing one, "No category" or a new one
   typed in the picker), **When** they tap Undo, **Then** the category shown is the one before
   that choice; Redo chooses it again.
10. **Given** the reader toggled Favorite, **When** they tap Undo, **Then** the favorite marker is
    back to what it was; Redo toggles it again.
11. **Given** the reader changed the category and then the text, **When** they tap Undo twice,
    **Then** the text change is undone first and the category change second.

---

### User Story 3 - See the title when reading a note (Priority: P2)

A reader opens a note to read it. At the top of the note, above the text, the title is shown in
large type, so it is clear which note is open without going back to the list. The title is shown
only when the note is read, not while editing (where it is the Title field).

**Why this priority**: Since feature 004 every note has an explicit title that may differ from the
first line of the text, so the reader page currently hides information the reader chose. It is a
small, self-contained change and lower than the editing tools only because it adds no new
capability.

**Independent Test**: Open a note titled "Groceries" whose text begins with a different line.
Verify "Groceries" is shown above the text in the reader, once, and is not shown as part of the
text. Open the editor and verify the Title field is shown there and no separate title heading.

**Acceptance Scenarios**:

1. **Given** a note with title "Groceries", **When** the reader opens it to read, **Then** "Groceries"
   is shown at the top of the content, larger or bolder than the body text, before the first line
   of the text.
2. **Given** the title is longer than one line, **When** the note is read, **Then** the title wraps
   onto further lines and is shown in full; it is never cut off with an ellipsis.
3. **Given** the note text starts with a line equal to the title (including a leading `#` heading
   marker), **When** the note is read, **Then** the title block is still shown, and the text is
   shown unchanged below it, so the same words appear twice; the app neither hides the title block
   nor removes or merges text.
4. **Given** the reader changes the reading font size (A- / A+), **When** the note is shown,
   **Then** the title follows the chosen size consistently with the text.
5. **Given** a long note, **When** the reader pages through it with the page buttons or edge taps,
   **Then** the title scrolls away with the content like the first lines of text do, and is not
   fixed on screen.
6. **Given** the reader taps Edit, **When** the editor opens, **Then** the title is shown in the
   Title field and not additionally as a heading, and after saving and returning to the reader the
   changed title is shown.
7. **Given** a note whose title was changed on another device and synced, **When** the reader
   opens it, **Then** the title shown is the synced one.

---

### User Story 4 - Select and copy text while reading (Priority: P2)

A reader reading a note wants to take a passage with them, for example a phone number, a recipe
step or a quote, into another note. On the reading page they select the text they want, tap Copy,
leave for another note's editor and paste it there. The reading page stays read-only: it offers
select and Copy only, no Cut, Paste or Delete.

**Why this priority**: Without it, a reader has to open the editor of the source note just to copy
from it, risking an accidental change. It builds on the editor's clipboard (User Story 1) and is
lower than the editing tools themselves.

**Independent Test**: Open a note on the reading page, select one sentence, tap Copy, open a
different note's editor and tap Paste: the sentence is inserted. Verify the first note was not
changed and the reading page offered no Cut, Paste or Delete.

**Acceptance Scenarios**:

1. **Given** a note is open on the reading page, **When** the reader selects a range of text,
   **Then** the selection is visibly marked and Copy becomes available.
2. **Given** no text is selected on the reading page, **When** the reader looks at Copy, **Then** it
   is unavailable and does nothing when tapped.
3. **Given** text is selected on the reading page, **When** the reader taps Copy, **Then** the
   selected text is remembered for pasting, the selection stays visible, and the note is unchanged.
4. **Given** text was copied on the reading page, **When** the reader opens any note's editor and
   taps Paste, **Then** the copied text is inserted (and in the Title field with line breaks
   replaced by spaces, as in User Story 1).
5. **Given** the reader selects text on the reading page, **When** they choose Select all, **Then**
   all text of the note, including the title, is selected.
6. **Given** a selection exists, **When** the reader taps outside the selection, changes the font
   size, or leaves the page, **Then** the selection ends; turning the page with the page buttons or
   the edge taps keeps it.
7. **Given** a link, a task box or an attachment is shown in the note, **When** the reader taps it
   outside a selection, **Then** it behaves as before (the link opens after its usual prompt, the
   task box toggles, the attachment opens); selecting does not trigger them.
8. **Given** the reader selects a range spanning headings, list items or several paragraphs,
   **When** they tap Copy, **Then** the whole range is copied as the note's own source text,
   including its line breaks and its formatting marks (for example `# `, `- `, `- [ ] `, `**`).
9. **Given** a heading, a bold phrase or a task item is selected completely, **When** the reader
   copies it, **Then** the copy includes that element's own marks (`# Title`, `**bold**`,
   `- [ ] task`), so pasting it into an editor reproduces the same formatting.
10. **Given** only part of a formatted element is selected (for example two words inside a bold
    phrase), **When** the reader copies it, **Then** only the selected characters are copied,
    without the marks of the element they were cut from.

---

### Edge Cases

- The caret is in the title and Paste inserts text containing line breaks: they become single
  spaces. Text pasted into the title that would exceed the 255-character title limit is refused
  with the existing clear message; nothing is inserted and the title is kept.
- A very large piece of text is cut or pasted: the editor stays usable and Undo restores it
  completely.
- Select all in an empty field selects nothing; with nothing selected the menu never offers Cut, Copy or Remove.
- Copy with a selection that ends in the middle of a word, an attachment marker line, a task box
  marker or a table row: the characters are copied exactly as they are in the text. Pasting an
  attachment marker line duplicates the marker; it is the reader's responsibility, and the existing
  rules for attachments apply on save.
- Undo or Redo is tapped while a selection exists: the selection is dropped or restored as part of
  the step, and the text ends in a consistent state.
- The keyboard is open and the commands are used: the commands remain reachable and visible, and
  the keyboard does not cover them; the text with the caret stays in view.
- The reader switches between the title and the text: each field keeps its own selection state, but
  the commands act on the field that currently has the caret, and Undo and Redo follow one shared
  history (see User Story 2, scenario 6).
- The device is rotated: the commands and the reader title keep working and the title is laid out
  for the new width.
- The note being read has an empty or whitespace-only title (an older note): nothing is shown in
  place of the title, and no empty gap is left.
- The reader leaves the editor: the copied text stays available, and Paste works in the next
  note's editor.
- The reader's system offers no way to share copied text with other apps: Copy and Paste still work
  inside the app, and nothing fails or shows an error because sharing is unavailable.
- Text arrives from another app and is pasted: it is inserted as plain text only; control
  characters other than line breaks and tabs are dropped, and an unusually large clipboard (over
  1,000,000 characters) is refused with a clear message instead of freezing the editor. Pasting
  into the Title field follows FR-008.
- The clipboard holds something that is not text (for example an image): Paste is unavailable.
- The editing history is discarded when the editor closes; it is never saved with the note or
  synced.
- A selection on the reading page crosses a page break: the whole range is selected and copied,
  and the reader can keep extending it with a marker after turning the page.
- Taps near the left and right edge of the reading page still turn the page; they do not start or
  change a selection.
- The reading page shows an image or other attachment inside the selected range: the attachment's
  marker line from the source text is copied like any other line (it is not the picture).
- A link is selected: its source text (the link as written, including a Markdown link's text and
  address) is copied, not the displayed link text alone.
- A table row, an attachment or an empty line on the reading page is selected as a whole, not by
  character: a selection that reaches it includes all of it, and Copy returns its line as written.
  Characters inside a table cell cannot be selected separately.
- The note is reloaded while it is open on the reading page (for example an attachment finished
  downloading): the selection ends and the reading position is kept.

## Requirements *(mandatory)*

### Functional Requirements

**Selection and commands**

- **FR-001**: In the editor the reader MUST be able to select a range of characters, in the Title
  field and in the note text, so that the selection is visibly marked.
- **FR-002**: The reader MUST be able to select all characters of the field that has the caret in
  a single step.
- **FR-003**: The editor MUST offer Copy, Cut, Paste and Remove (delete the selection) in a context
  menu, without a physical keyboard. The menu opens on a long press (on a word: with that word
  selected; on empty text: without a selection) and next to the selection; the editor's header MUST
  keep only Cancel, Save, Favorite, Attach and Delete (the note).
- **FR-004**: Copy MUST remember the selected characters without changing the field. Cut MUST
  remember them and remove them. Delete MUST remove them without changing what was remembered.
- **FR-005**: Paste MUST insert the remembered characters at the caret, or replace the selection
  if one exists, and MUST leave the caret after the inserted characters.
- **FR-006**: The menu MUST list Cut, Copy and Remove only while there is a selection, and Paste only
  while something has been copied or cut; commands that do not apply are not shown.
- **FR-007**: The remembered text MUST be shared by the Title field, the note text, the reading
  page and all notes, so text can be moved between them. Where the reader's system offers a
  clipboard shared with other apps, Copy and Cut MUST place the text there and Paste MUST take the
  text from there, so text can also move into and out of other apps. Where no such clipboard is
  available, the remembered text is kept inside the app only, with no error.
- **FR-032**: Text taken from outside the app by Paste MUST be treated as untrusted input: only its
  plain text is used, control characters other than line breaks and tabs are removed, and text over
  1,000,000 characters is refused with a clear message, keeping the field unchanged.
- **FR-033**: The app MUST put text on the shared clipboard only when the reader taps Copy or Cut,
  and MUST NOT read it for any other purpose than a Paste the reader asked for. Copied text MUST
  NOT be written to logs, to the saved notes, or to the server.
- **FR-008**: Text pasted into the Title field MUST have line breaks replaced by single spaces, and
  a result longer than 255 characters MUST be refused with a clear message, keeping the previous
  title. The existing title rules (trimmed spaces, no control characters) stay in force.
- **FR-009**: Typing while a selection exists MUST replace the selection; tapping elsewhere MUST
  end it.
- **FR-010**: Using the commands MUST NOT make the on-screen keyboard disappear unexpectedly or
  lose the caret position, and the menu MUST stay visible and reachable while the keyboard is open
  (it is placed above the selection, or below it when there is no room above, and never behind the
  keyboard).
- **FR-034**: A selection MUST show a marker at its start and at its end that the reader can drag to
  move that end of the selection (on the reading page also across blocks); the menu MUST be hidden
  while a marker is dragged and shown again when it is let go. A tap outside the selection ends it,
  hides the markers and the menu.

**Undo and redo**

- **FR-011**: The editor MUST offer Undo and Redo in the same context menu, listed only when a step can
  be undone or redone respectively.
- **FR-012**: Undo MUST reverse the most recent editing step: a run of typing, a paste, a cut, or a
  delete. Redo MUST reapply the most recently undone step. Typing MUST be grouped into steps larger
  than one character (for example a word or a pause), so undoing typing needs few taps.
- **FR-013**: Undo and Redo MUST cover the Title field, the note text, the category and the
  favorite marker in one shared history, undone in the reverse order of the edits. For a title or
  text step the caret is restored to where the step happened. Choosing a category (including
  "No category" or a newly typed one) and toggling Favorite are each one step.
- **FR-014**: A new edit after an Undo MUST discard the steps that could have been redone.
- **FR-015**: The history MUST start when the editor opens; the content of the note at that moment
  MUST NOT be removable by Undo. The history MUST be discarded when the editor closes, MUST NOT be
  saved with the note, and MUST NOT be synced.
- **FR-016**: Changes by attachments (adding an attachment marker line) MUST be part of the history
  like other edits to the text.
- **FR-017**: When Undo has returned the title, the text, the category and the favorite marker to
  what they were when the editor opened, the editor MUST NOT treat the note as having unsaved
  changes. The existing rule that unsaved changes ask the reader to save or discard
  stays otherwise in force.
- **FR-018**: Undo, Redo and the editing commands MUST work with no network connection and MUST NOT
  change what has been saved until the reader saves; saving works as before (feature 004, FR-004
  to FR-008).

**Selecting and copying on the reading page**

- **FR-026**: The reading page MUST let the reader select a range of the note's text, including
  the title, with a visible mark and across paragraphs, headings and list items.
- **FR-027**: The reading page MUST offer Copy and Select all in a context menu that opens on a long
  press (on a word, a table row, an attachment or a blank line) and shows a marker at each end of
  the selection, which can be dragged. Its footer keeps only the page buttons. The reading page
  MUST NOT offer Cut, Paste, Remove, Undo or Redo, and MUST NOT change the note.
- **FR-028**: Copy on the reading page MUST put the selected text into the same remembered text
  that the editor uses (FR-007), so it can be pasted into any note's editor.
- **FR-029**: Selecting on the reading page MUST NOT trigger links, task boxes, attachments or the
  edge tap zones for page turning; those keep their existing behavior when tapped outside a
  selection.
- **FR-030**: A selection on the reading page MUST end when the reader taps outside the selection,
  changes the font size, reloads the note or leaves the page. Turning the page MUST NOT end it, so
  a range can be extended across pages.
- **FR-031**: What Copy puts there on the reading page MUST be the note's source text for the
  selected range, not the displayed rendering: line breaks and formatting marks inside the range
  are kept; the marks of an element that is selected completely are included; the marks of an
  element that is only partly selected are not added. Characters shown by the app but not part of
  the text (the title block, task box drawings, link prompts, attachment previews) MUST NOT be
  copied as such; a selected title is copied as its title characters only.

**Title in the reader**

- **FR-019**: When a note is read (not edited), the reader page MUST show the note's title at the
  top of the content, above the first line of the text, in a larger or bolder style than the body
  text.
- **FR-020**: The title MUST be shown in full: long titles wrap and are never truncated.
- **FR-021**: The title MUST follow the reader's chosen font size and scroll away with the content
  when the reader pages through the note.
- **FR-022**: The title block MUST be shown whether or not the text begins with a line that repeats
  the title, and the text MUST NOT be altered or de-duplicated in the reader because of it. The
  title is displayed separately and the text below is exactly as saved.
- **FR-023**: The editor MUST NOT show the title as a heading in addition to the Title field.
- **FR-024**: A note without a visible title (empty or whitespace-only) MUST show no title block
  and no extra gap in the reader.
- **FR-025**: The title shown in the reader MUST be the one shown in the list and the editor for
  that note, including a title changed by sync.

### Key Entities *(include if feature involves data)*

- **Selection**: A contiguous range of characters in the Title field or the note text of the editor
  that has the caret, or in the text shown on the reading page. It exists only while that page is
  open.
- **Clipboard text**: The characters most recently copied or cut, in this app or, where the reader
  allows, in another app. It is shared by the title and the text of all notes and the reading page,
  and is not stored with any note or synced by this app.
- **Edit step**: One undoable change to the title, the text, the category or the favorite marker
  (a run of typing, a paste, a cut, a delete, a category choice or a favorite toggle). Steps form an ordered history that exists only while the editor is open.
- **Note**: Unchanged by this feature; the title and the text of a note are saved and synced as
  before. Nothing about selection, clipboard text or history is added to a note.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A reader can select a word, copy it and paste it elsewhere in the same note in at
  most 4 taps (select the word, Copy, place the caret, Paste) and with zero typed characters.
- **SC-002**: In 100% of test cases, Cut followed by Paste at the same position, and Delete followed
  by Undo, restore the exact original text and caret position.
- **SC-003**: A reader can recover from an accidental Cut, Paste or Delete with one tap on Undo
  in 100% of test cases, in the title and in the text.
- **SC-004**: Undoing typing of a 40-character sentence typed continuously takes at most 3 taps on
  Undo, and Redo returns it in the same number of taps.
- **SC-005**: After any sequence of Undo and Redo taps that returns the note to its opened state,
  the editor treats the note as unchanged in 100% of test cases.
- **SC-006**: Each editing command (copy, cut, paste, delete, undo, redo) shows its result within
  1 second on the reader for a note of a few pages, and the editor stays usable (the same limit
  for typing and the commands) for a note of 100,000 characters.
- **SC-007**: In 100% of test notes, the reader page shows the note's title block above the text
  exactly once, and the text below matches the saved text, also for notes whose first line repeats
  the title.
- **SC-008**: A title of 255 characters is shown in full in the reader at every font size, without
  being cut off.
- **SC-009**: A reader can copy a sentence from the reading page and paste it into another note in
  at most 6 taps (select, Copy, Back, open the other note's editor, place the caret, Paste) and
  with zero typed characters.
- **SC-010**: In 100% of test cases, selecting and copying on the reading page leaves the note's
  title, text and modification time unchanged and starts no link, task or attachment action.

## Assumptions

- Scope is the editor and the reading page: select, copy, cut, paste, delete, undo and redo are
  provided for the Title field and the note text in the editor; the reading page gains the title
  block (User Story 3) and read-only select and Copy (User Story 4, confirmed in clarification).
- The commands are in a context menu next to the selection, like the reader's own (the author's
  screenshot of its selection bubble and markers is the model); no reliance on keyboard shortcuts,
  which the reader's on-screen keyboard cannot provide. The menu and the markers must work with a
  finger on the e-ink screen and be reachable while the keyboard is open (the menu sits above the
  selection, or below it when there is no room above).
- The clipboard is shared with other apps on the reader where the device supports it (confirmed in
  clarification); copying is an explicit tap by the reader, so this does not conflict with the
  project's privacy rule. Whether the PocketBook system provides a clipboard shared across apps is
  to be established in planning; if it does not, the in-app clipboard is the delivered behavior
  and this assumption is recorded in the plan. Copied text is never written to logs, to the saved
  notes or to the server by this app.
- The undo history has no fixed step limit that the reader would notice during normal use (at least
  100 steps are kept); the oldest steps may be dropped beyond that.
- Typing is grouped into steps by word or by pause, as in common text editors; the exact grouping
  is decided in planning.
- The title shown in the reader uses the same text as the list and the editor; its look (larger or
  bold, with the same black-on-white high contrast as the rest of the app) is decided in
  planning.
- The earlier behavior that the reader page starts directly with the first line of the text
  (feature 001) is extended by the title block and by select and Copy; nothing else on the reading
  page changes.
- Out of scope: rich-text formatting commands, find and replace, drag-and-drop of text, text
  editing on the reading page, a clipboard history or picker, and any change to saving, syncing, categories
  or conflicts.
