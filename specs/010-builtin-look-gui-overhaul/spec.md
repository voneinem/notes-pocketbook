# Feature Specification: Built-in Look (GUI Overhaul to Match the Reader's Own Apps)

**Feature Branch**: `feature/010-builtin-look-gui-overhaul`

**Created**: 2026-10-13

**Status**: Draft

**Input**: User description: "GUI overhaul - match with built-in apps - see 10 screenshots in screens folder on device. Also the menu icon which has a frames should better match by having no frame."

## Reference

The ten screenshots in `screens/` on the device (`scr0001`-`scr0010`) show the reader's own apps and
are the reference for this feature. What they establish, in the words used below:

| Shot | Shows | What to take from it |
|------|-------|----------------------|
| 0001 | Application menu (home) | Every app is a frameless line-art icon over its name; "PB Notes" is the only icon drawn inside a heavy square frame |
| 0002 | Built-in Notes list | A top bar (home, menu, centred upper-case title, search, new), a filter/sort row with drop-down labels and a layout toggle, an upper-case count line, rows with a thumbnail, a bold title and a "when • pages" line, hairline row separators |
| 0003 | Built-in note editor | A single row of line icons on top (back, tools, undo, redo), a thick bar under the active tool, a small zoom/page strip at the bottom |
| 0004, 0006, 0007 | Pop-up menus | A rounded pop-up with a heavy outline and a pointer to its origin; rows of icon + label (or icons only) separated by dotted hairlines |
| 0005 | "New note" dialog | A rounded dialog with a heavy outline, a bold title and close icon over a rule, labelled fields with thin rounded outlines, a two-state toggle with a black selected half, picture tiles with a thick outline on the selected one, and a footer of two full-width upper-case text buttons split by a rule; the page behind is dimmed |
| 0008 | Reader with tool panel | A top bar (back, contents, title, search, bookmark), a bottom panel of icon + label tools, a slider with a ring knob, and a "page / total" line |
| 0009 | Reader settings panel | Tabs of icon + label with a thick bar under the active tab, picture tiles for choices with a thick outline on the selected one, an "on/off" switch, round minus/plus buttons, a slider with a ring knob |
| 0010 | Contents list | A top bar (back, upper-case title), icon tabs with a thick bar under the active tab, rows with hairline separators and right-aligned numbers, a scroll bar on the right edge, a footer with page-up/page-down and one wide text button |

## Clarifications

### Session 2026-10-13

- Q: In the reader and editor, are actions icon-only or icon + label? → A: Mixed. Universal actions (back, search, undo/redo, favorite star, save, close) are icon-only in the top bar; the remaining actions (attach, delete, category, title, text size) are icon + label tools in a bottom panel.
- Q: Should the note list also get the built-in Notes app's List/Tile layout toggle? → A: Yes. The note list offers a List and a Tile layout with a toggle icon in the filter/sort row; the choice is remembered, as in the attach browser.
- Q: Is the overhaul released as one update or in slices? → A: In slices, in priority order, one update per user story (or per pair of stories); each slice is checked on the device before the next starts.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - The app's icon fits in the application menu (Priority: P1)

The reader opens the application menu (the grid of apps). Today the PB Notes icon is the one icon that
sits inside a heavy square frame; every other app shows a frameless line drawing. The reader should find
PB Notes in the menu as an icon of the same kind as its neighbours: the notes glyph on its own, with no
frame around it, at the same visual size and line weight as the built-in icons.

**Why this priority**: It is the first thing the reader sees every time they start the app, it is a single
self-contained asset, and it is the one item the request names explicitly. It delivers visible value on its
own and can ship before any screen is touched.

**Independent Test**: Install the app and open the application menu. Compare the PB Notes icon with the
icons of the built-in apps (Notizblock, Rechner, Wörterbuch) next to it: there is no square frame, the
drawing is line art of similar weight and size, and it is as legible as its neighbours. Also check the
second icon state the menu uses (the "pressed/selected" variant) and verify it has no frame either.

**Acceptance Scenarios**:

1. **Given** the application menu is shown, **When** the reader looks at the PB Notes entry, **Then** its
   icon has no surrounding frame or border of any kind.
2. **Given** the icon beside built-in icons, **When** compared, **Then** its drawn area, stroke weight and
   vertical position over the label match the neighbouring icons closely enough that it no longer stands
   out as heavier or larger.
3. **Given** the reader taps the icon, **When** the menu shows its pressed/selected state, **Then** that
   state is also frameless and is still clearly distinguishable from the normal state.
4. **Given** the icon is shown in the application menu, **When** it is viewed at the size used there,
   **Then** the notes glyph remains recognizable (a page with a header bar and text lines).

---

### User Story 2 - The note list looks and behaves like the built-in Notes list (Priority: P1)

The reader opens PB Notes and sees a screen laid out like the reader's own Notes app: a top bar with a
home/back control, the screen title centred in upper case, and icon controls for search and new note on the
right; below it one row for the view filter and the sort order, shown as labels with a small drop-down
arrow (for example "Alle Notizen ↓" and "Zuletzt geändert ↓"); then an upper-case count line ("12 NOTIZEN");
then the notes, each row showing the title in bold and a quieter line with the category and date, with a
hairline between rows. At the right end of the filter/sort row, a toggle icon (as in the built-in Notes
app) switches the list between this List layout and a Tile layout in which each note is a card with its title,
category, date and favorite marker, several cards to a row; the app remembers the last choice. The sync controls and status that exist today stay available but no longer take the
place of the title as large bordered buttons.

**Why this priority**: The note list is the screen the reader sees most and the one that has a direct
built-in counterpart (0002), so it carries most of the "feels like part of the device" effect.

**Independent Test**: Open the note list with a mix of notes (favorites, categories, long titles, no
category). Side by side with the built-in Notes list, verify the structure matches (top bar, filter/sort
row, count line, rows with hairlines), then use search, change the view, change the sort order, create a
note and start a sync, and verify every one of those still works and is reachable in at most two taps.

**Acceptance Scenarios**:

1. **Given** the note list is shown, **When** the reader looks at the top, **Then** they see a top bar with
   the centred upper-case title and icon controls (search, new note), without framed text buttons.
2. **Given** the filter/sort row, **When** the reader taps the view label, **Then** the list of views
   (All, Favorites, Uncategorized, each category with its count) appears in a pop-up styled like the
   device's own, and choosing one filters the list.
3. **Given** the filter/sort row, **When** the reader taps the sort label, **Then** they can choose between
   the existing orders and the label shows the active one.
4. **Given** notes are listed, **When** the reader looks at a row, **Then** the title is bold, the category
   and date form a quieter second line, a favorite is marked, and rows are separated by hairlines; the
   row remains large enough to hit reliably with a finger.
5. **Given** the reader taps the search icon, **When** the search field appears, **Then** typing filters the
   list as today and there is an obvious way to close search and clear the filter.
6. **Given** a sync is running or has failed, **When** the reader looks at the screen, **Then** the sync
   state and the number of unsynced changes are still shown, and "Sync now" and Settings are still
   reachable.
7. **Given** a long list, **When** it is longer than one page, **Then** it is still paged with the page
   buttons (no kinetic scrolling) and the paging controls are styled like the device's own.
8. **Given** the note list is in List layout, **When** the reader taps the layout toggle icon, **Then** the
   same notes (same view, sort order and search) are shown as tiles, several to a row, each tile showing the
   title, category, date and favorite marker, and the icon now shows that a tap returns to the list.
9. **Given** the Tile layout, **When** the reader taps a tile, **Then** the note opens exactly as from a row;
   views, sort, search, sync and paging work the same in both layouts.
10. **Given** the reader left the note list in a given layout, **When** they close and restart the app,
    **Then** the list starts in that layout.

---

### User Story 3 - Dialogs and pop-up menus look like the device's own (Priority: P2)

Every dialog and pop-up in the app takes on the look of the device's: dialogs (new category, title entry,
confirmations, the choice lists, the date/time picker, the emoji picker) are rounded with a heavy outline,
have a bold title with a close icon over a rule, use thin rounded outlines for fields, and end in a footer of
full-width upper-case text buttons separated by rules ("ABBRECHEN | SPEICHERN"). The page behind a dialog is
dimmed. Pop-up menus (the editor's text-selection menu, the "/" menu) are rounded with a heavy outline and a
pointer toward where they came from, with rows of icon and label separated by dotted hairlines, or icons only
where the built-in menu is icons only.

**Why this priority**: These elements appear on every screen and are highly visible, but they sit on top of
screens that already work, so they follow the main screens.

**Independent Test**: Trigger every dialog and pop-up the app has (a confirmation, a text entry, a choice list,
the date/time picker, the emoji picker, the selection menu, the "/" menu). In each, verify the outline,
title/close, field style, footer buttons and dimmed background match the device's dialog reference, and that
every action that was possible before is still possible.

**Acceptance Scenarios**:

1. **Given** any dialog is opened, **When** it appears, **Then** it has a heavy rounded outline, a bold
   title with a close icon above a rule, and the page behind it is dimmed.
2. **Given** a dialog with actions, **When** the reader looks at its footer, **Then** the actions are
   full-width upper-case text buttons divided by a vertical rule, and the dismissing action is on the left.
3. **Given** a dialog with a text field, **When** it is shown, **Then** the field has a thin rounded outline
   and a label above it, and the on-screen keyboard still opens and keeps the field visible.
4. **Given** the selection menu or the "/" menu is opened, **When** it appears, **Then** it is a rounded
   pop-up with a heavy outline and a pointer toward the text it acts on, and every entry it had before is
   still present and works.
5. **Given** any dialog or menu is open, **When** the reader presses the device's Back, **Then** it closes as
   before without changing anything.

---

### User Story 4 - The reader and editor screens use the device's bars and controls (Priority: P2)

The note reader and the note editor gain the same kind of top bar as the built-in reader: a back chevron on
the left, the note title centred, and icon controls on the right, with a hairline under the bar. Actions that
today are bordered text buttons (Back, Edit, Favorite, Attach, Save, Cancel, Delete and the like)
become line-art controls in the device's style: the universal actions (back, search, undo/redo, favorite
star, save, close) are icon-only in the top bar, and the remaining actions (attach, delete, category, text
size) are icon + label tools in a bottom panel; the note's title stays an input field under the top bar. Toggles
have a clearly visible state (for example a filled star for a favorite note). Page position is shown as
"page / total" at the bottom in the device's style, and paging controls match.

**Why this priority**: The reader and editor are where the reader spends their time, but they are bigger and
riskier to change than the list and the dialogs, and their built-in counterparts (0008, 0009) are reference
for the look rather than a 1:1 layout.

**Independent Test**: Open a note in the reader and then in the editor. Verify the top bar, the icon controls,
the bottom panel and the page indicator match the reference style, then perform every action the screen had
before (edit, save, cancel with unsaved changes, favorite on/off, attach, delete, change font size, page
forward and back) and verify each still works and is reachable.

**Acceptance Scenarios**:

1. **Given** a note is opened, **When** the screen appears, **Then** it has a top bar with a back chevron,
   the centred note title and icon controls, with a hairline beneath, and the note content fills the rest.
2. **Given** a toggle such as Favorite, **When** it is on, **Then** its icon is visibly filled or inverted, and
   when it is off it is the plain outline.
3. **Given** the reader has a text-size control, **When** it is shown, **Then** it uses the device's style:
   round minus/plus buttons, a slider with a ring knob and the current size as a number with its unit.
4. **Given** a note longer than one page, **When** the reader views it, **Then** a "page / total" indicator is
   shown at the bottom in the device's style and the paging controls move by page as today.
5. **Given** the editor has unsaved changes, **When** the reader leaves, **Then** the unsaved-changes
   confirmation appears as before, in the dialog style of User Story 3.

---

### User Story 5 - Settings, setup and information screens, and the attach browser, follow the same style (Priority: P3)

Settings, the first-run setup, the About screen, the license pages and the attach file browser get the same
top bar, upper-case title, hairline-separated rows, thin rounded fields, device-style switches and
buttons, and footer styling as the screens above, so that no screen of the app looks like a different
program. Where a screen has several groups of options it may use icon tabs with a thick bar under the active
one, as the built-in settings panel and contents list do.

**Why this priority**: These screens are visited rarely; they must not look foreign, but they do not shape the
daily experience.

**Independent Test**: Open each of these screens in turn and compare it with the reference style: top bar,
rows, fields, switches, buttons, selection outline. Run through setting up an account, changing a setting,
opening About and a license page, and attaching a file (including the List/Tile toggle and previews of feature
009), and verify each works as before.

**Acceptance Scenarios**:

1. **Given** any of these screens is opened, **When** it appears, **Then** it has the same top bar and the
   same hairline-separated rows as the note list, and no text button with a heavy rectangular border remains
   anywhere in the app.
2. **Given** an on/off option, **When** it is shown, **Then** it is a device-style switch with "on" shown
   inside the filled half, not a checkbox or a bordered button.
3. **Given** a choice among a few options shown as pictures or tiles (for example the attach browser's
   List/Tile layouts), **When** one is selected, **Then** it is marked with a thick outline.
4. **Given** the attach browser, **When** it is opened, **Then** its List and Tile views, previews, "Up" and
   "Cancel" behave exactly as specified in feature 009, only restyled.

---

### Edge Cases

- **Both orientations**: every restyled screen, dialog and pop-up still lays out correctly in portrait and in
  the rotated orientations, including with the on-screen keyboard open; nothing is clipped or overlaps the top
  bar.
- **Long titles and labels**: a very long note title, category name or file name is shortened with an
  ellipsis in the top bar and in rows instead of wrapping into the controls or pushing them off screen.
- **Empty states**: an empty list, an empty search result and an empty category each show a quiet message
  instead of a blank page, and the count line reads "0 notes" in the same style.
- **Tile content**: a note without a category, a very long title, or a title made only of emoji each render as a
  complete card without clipping or overlapping neighbouring cards; an odd number of notes leaves the last
  row partly empty without distortion.
- **Many categories**: the view pop-up with more categories than fit on screen is still fully reachable
  (paged or scrollable by the existing page buttons).
- **Touch targets**: icon-only controls are no smaller than the hit area of today's buttons so a finger can
  still hit them reliably; no action becomes harder to trigger by the restyle.
- **No colour**: all state (pressed, selected, disabled, on/off) is shown by weight, fill or outline, never by
  colour, because the panel is monochrome; disabled items stay recognisable as disabled in greyscale.
- **E-ink refresh**: no screen introduces animation, fades or transitions; a state change redraws once.
- **Language**: the app shows English labels only (it has no translations) and this feature adds none; the
  compact controls are sized for labels up to 30 % longer than today's, so a later translation (for example
  German) fits without clipping.
- **Unknown device chrome**: if the device's own status bar (clock, Wi-Fi, battery) is shown over the app, the
  new top bar sits below it without overlapping.
- **Existing data and sync**: the restyle changes how things look, never what is stored or synced; no note,
  setting or attachment is altered or lost by the update.

## Requirements *(mandatory)*

### Functional Requirements

**Application icon**

- **FR-001**: The application's menu icon MUST be drawn without any frame, border or box around the notes
  glyph.
- **FR-002**: The icon's drawn area, stroke weight and vertical placement MUST match the built-in apps'
  icons closely enough that it does not appear heavier or larger than its neighbours in the application menu.
- **FR-003**: The icon's second (pressed/selected) state MUST also be frameless and MUST remain visibly
  different from the normal state.
- **FR-004**: The notes glyph (a page with a header bar and text lines) MUST remain recognisable at the size
  shown in the application menu.

**Common look**

- **FR-005**: Every screen MUST use a top bar in the device's style: a back chevron (or home on the list) on
  the left, the screen or note title centred, icon controls on the right, and a hairline under the bar. Titles
  of screens (not of notes) MUST be shown in upper case.
- **FR-006**: Controls MUST be line-art icons (icon-only for universal actions, icon over a label for the
  rest, per FR-017), in a single stroke weight consistent
  with the device's icons. Heavy rectangular text buttons MUST NOT remain anywhere in the app; text-only
  actions appear only as footer buttons of dialogs (FR-014) or as one wide action button where the reference
  uses one.
- **FR-007**: Lists MUST use rows separated by hairlines (dotted or solid, as in the reference), with the
  primary text in regular or bold weight and secondary text quieter, and numbers or markers aligned to the
  right edge.
- **FR-008**: State MUST be shown without colour: pressed, selected, active, on/off and disabled are
  distinguished by outline weight, fill, an underline bar or greying, and a toggled-on icon MUST look
  different from the same icon toggled off.
- **FR-009**: Selection among a few visual options (layouts, templates) MUST be marked with a thick outline on
  the chosen one; selection among tabs MUST be marked with a thick bar under the active tab; a two-state
  choice MUST be a toggle with the chosen half filled black.
- **FR-010**: Typography (typeface family, relative sizes for title, body and caption, weights) MUST be
  visually consistent with the device's own apps, and the type size of controls MUST be no smaller than today's.
- **FR-011**: Screens MUST NOT use animation, fades or transitions.

**Note list**

- **FR-012**: The note list MUST provide: a top bar with the upper-case title and icon controls for search and
  new note; a filter/sort row with the view and the sort order shown as labels with a drop-down arrow; an
  upper-case count of the notes shown; a List/Tile layout toggle icon at the right end of the filter/sort row;
  and, in List layout, rows with a bold title and a quieter category/date line.
- **FR-012a**: In Tile layout each note MUST be a card showing its title (shortened with an ellipsis if
  long), category, date and favorite marker, laid out several to a row; the chosen layout MUST be remembered
  across app restarts and MUST apply to the same notes, view, sort order and search as the List layout.
- **FR-013**: Every function of today's list MUST remain available: choose a view (All, Favorites,
  Uncategorized, each category with its count), choose the sort order, search with clearing, create a note,
  open a note, start a sync, read the sync state and the count of unsynced changes, open Settings, and page
  through a long list with the page buttons, in both layouts.

**Dialogs and pop-up menus**

- **FR-014**: Dialogs MUST be rounded with a heavy outline, a bold title with a close icon over a rule, thin
  rounded outlines for fields with a label above, and a footer of full-width upper-case text buttons divided by
  a rule, the dismissing action first. The page behind MUST be dimmed.
- **FR-015**: Pop-up menus (text selection, "/" menu, view and sort choices) MUST be rounded with a heavy
  outline and a pointer toward their origin, rows MUST show an icon and a label separated by dotted hairlines,
  and menus that are icon-only in the reference MAY be icon-only if each icon is unambiguous.
- **FR-016**: Every dialog and pop-up that exists today (confirmations, text entry, choice lists, date/time
  picker, emoji picker, selection menu, "/" menu, conflict notice) MUST be restyled, and each MUST still offer
  every action and result it offers today; the device's Back MUST still close it without effect.

**Reader and editor**

- **FR-017**: The reader and the editor MUST show the top bar of FR-005 with the note title centred, and their
  actions MUST be line-art controls: back, search, undo/redo, favorite, save and close are icon-only in the
  top bar; attach, delete, category and text size are icon + label tools in a bottom panel, and the title is
  an input field under the top bar (an input, not an action). Toggles
  MUST show their state per FR-008.
- **FR-018**: The reader MUST show the current position as "page / total" at the bottom, and its text-size
  control MUST use round minus/plus buttons, a slider with a ring knob and the current size with its unit.
- **FR-019**: Leaving the editor with unsaved changes MUST still ask for confirmation, in the dialog style of
  FR-014.

**Other screens**

- **FR-020**: Settings, setup, About, license pages and the attach browser MUST use the common look (FR-005 to
  FR-011) with device-style switches for on/off options and the dialog and list styles above, and the attach
  browser MUST keep every behaviour of feature 009.

**No regression**

- **FR-021**: The restyle MUST NOT change the content or behaviour of stored notes, sync, attachments,
  settings, or the on-screen keyboard behaviour; no note, setting or attachment is altered by the update.
- **FR-022**: Every restyled screen MUST work in portrait and in the rotated orientations, with and without the
  on-screen keyboard, without clipped or overlapping controls.
- **FR-023**: Touch targets MUST NOT be smaller than those of today's buttons, and no function MUST need more
  taps than before except where the change is to move it into one overflow pop-up that the reference also uses.
- **FR-024**: The overhaul MUST be deliverable in slices in the priority order of the user stories (icon, note
  list, dialogs and menus, reader and editor, remaining screens). After every slice the whole app MUST be fully
  working and releasable: a screen not yet restyled keeps its current look and behaviour, and the style
  elements shared by restyled and not-yet-restyled screens MUST NOT make either unusable.

### Key Entities

- **Look reference**: the ten device screenshots, the single source of truth for what "matches the built-in
  apps" means. It defines the elements above (top bar, rows, dialog, pop-up, tabs, switch, slider, tiles) but
  is not itself stored by the app.
- **Application icon**: the picture the application menu shows for PB Notes, in a normal and a pressed/selected
  state.
- **Style elements**: the recurring pieces (top bar, icon control, row, dialog, pop-up menu, tab, toggle,
  switch, slider, tile) from which every screen is assembled so that the whole app reads as one style.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In the application menu, the PB Notes icon has no frame, and a reviewer who compares it with the
  built-in icons beside it cannot name it as the odd one out in a blind check against three neighbouring icons.
- **SC-002**: For each of the screens covered by the reference (note list, editor, reader, dialog, pop-up menu,
  settings panel), a side-by-side check against its screenshot finds every listed structural element present
  (top bar content, row layout, footer, selection marking); zero listed elements are missing.
- **SC-003**: No bordered rectangular text button remains anywhere in the app: a walk through every screen
  finds none.
- **SC-004**: Every action that existed before the overhaul is still reachable and works: 100% of the
  previous device check list (notes, search, views, sort, sync, editor actions, attach, settings, setup) passes
  after the change.
- **SC-005**: A reader can create a note, find it with search and open it in no more steps than before the
  change (at most the same number of taps).
- **SC-006**: All restyled screens render without clipped, overlapping or unreachable controls in all four
  device orientations and with the keyboard open, in a check of every screen in every orientation.
- **SC-007**: The note list in List layout shows at least as many notes per page as today, and in Tile layout
  at least as many as in List layout. The reader shows at least as much text per page as today at the same
  text size.
- **SC-008**: A screen state change (open a dialog, toggle a control, switch a tab) redraws without any visible
  intermediate animation frame.
- **SC-009**: Existing notes, settings and attachments are all present and unchanged after updating the app,
  and a sync before and after the update produces no change on the server.
- **SC-010**: After each slice, the full previous device check list passes on the reader before the next slice
  starts; no slice is shipped with a known failing item.

## Assumptions

- "The menu icon which has a frame" is the PB Notes launcher icon in the application menu (screenshot 0001),
  the only app icon drawn inside a heavy square frame. The change is to remove that frame; the notes glyph
  itself is kept.
- "Match with built-in apps" means match the look and the structure of the device's built-in Notes app, reader
  and its dialogs and menus as shown in the ten screenshots; it does not mean copying their features. The
  built-in editor's drawing tools (pen, eraser, templates, handwriting recognition) have no counterpart in this
  app and are out of scope.
- Where the screenshots show no direct counterpart (for example the Settings screen of this app), the nearest
  built-in pattern is used: top bar + hairline rows + device-style switches.
- The device's own system status bar (clock, Wi-Fi, battery) is drawn by the system, not by this app; this
  feature neither adds nor removes it, but the app's top bar is placed below it when it is shown.
- The app's labels are English only today and stay so; the reference screenshots are German, so the wording
  differs from them and only the look is compared. No translation is added by this feature.
- New icons are drawn in the project's own line-art style for this feature (their licence is the project's
  own); the device's built-in icon files are not copied.
- Release is sliced (clarified): one update per user story or pair of stories, each device-checked before the
  next. Until the last slice ships, the app may mix the new and the old look between screens; that is accepted,
  but each slice must stand on its own (FR-024).
- The only new behaviour beyond restyling is the note list's Tile layout and its remembered setting (clarified);
  notes have no thumbnails, so tiles show text only (no preview image).
- Behaviour, data formats, sync, and the editor's content model are unchanged; this is a presentation change.
- Feature 009 (attach file browser) is already built and is restyled here, not re-specified.
- The device used for the final check is the same PocketBook model the earlier features were verified on.
