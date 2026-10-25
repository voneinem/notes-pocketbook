# Changelog

One entry per release, newest first. The heading is the content of `VERSION`; the release is tagged
`v<version>` after the merge. `scripts/check-version-bump.sh` (part of `scripts/pre-merge.sh`) fails when a
branch changes the product without raising `VERSION` and adding the entry here.

## 1.10.1

- The install package now follows the PB Store package contract, so the PB Store can list PB Notes: it has a
  `manifest.json` at its top level (made at build time from a template and the version), is written by the store's
  own packing tool, is built twice and compared to prove it is reproducible, and passes the store's package check.
  `INSTALL.md` is no longer inside the package; it stays in the repository and is linked from each release. The files
  under `applications/` and the package name are unchanged, so installing by hand works as before.
- New `scripts/check-release.sh` checks that the release tag, `VERSION`, the package name and the package's own version
  agree before a release is published.
- No change to the app, to notes, sync or the data.

## 1.10.0

- Settings, set-up, About, the license pages and the attach file browser now look like the rest of the app and the reader's
  own screens: a top bar with a Back chevron and the title in capitals, rows split by dotted dividers, and labelled fields
  with thin rounded outlines. **Auto Sync** is a pill switch showing *on* or *off*; the screen rotation, *Forget account*,
  *About this app*, *License* and *Third-party licenses* are tappable rows. On the set-up screen *Test connection* and
  *Save* are a footer above the keyboard and the password is shown or hidden with an eye icon.
- Attach browser: *Cancel*, *Up* and the List/Tile toggle are icons in the top bar; browsing, previews and attaching work as
  before.
- No bordered text button is left anywhere in the app. The attachment actions *Download* and *Open* are tools with an icon.
- No change to notes, sync or the data.

## 1.9.0

- Reader: a top bar with **Back**, the note's title and **Edit** (a pencil); the category and the favorite star are one
  quiet line above the text. The bottom bar has the **Text size** tool and the page controls with the position as
  "3 / 12". Text size is a slider with round minus and plus buttons and the size in points (7, 9 or 12 pt); the three
  sizes and the stored setting are the same as before (the A- and A+ buttons are gone). The text area is as high as before.
- Editor: a top bar with **Back** (asks first when there are unsaved changes), **Undo**, **Redo**, the **Favorite** star
  (filled when marked) and **Save** as icons; the title is a labelled field; **Attach**, **Category** and **Delete** are
  tools in a panel directly above the keyboard, so none needs an extra tap. The category list opens as a bubble over the
  Category tool without hiding the keyboard.
- Undo and Redo, formerly only in the long-press menu, are now also icons of the editor's top bar.
- No change to notes, sync or the data.

## 1.8.0

- Dialogs and pop-up menus now look like the reader's own. Dialogs (confirmations such as *Discard changes?* and *Delete
  this note?*, the text entry for a new category, the date and time picker, the emoji grid) have a heavy rounded outline,
  a bold title with a close icon over a rule and a footer of full-width buttons split by a rule (the one that dismisses
  on the left, e.g. KEEP EDITING | DISCARD); the page behind is dimmed. Pop-up menus (the text selection menu, the "/" menu,
  and the lists of views, sort orders, the note list's menu and categories) are rounded bubbles with a pointer to what they
  belong to; the selection menu shows icons, the "/" menu an icon and a label per row with dotted dividers.
- Note list: the sort order is chosen from a pop-up like the view, instead of toggling on each tap.
- What the dialogs and menus offer has not changed. No change to notes, sync or the data.

## 1.7.0

- Note list: the list now looks like the reader's own Notes app. A top bar with **Home** (leaves the app), **Menu**
  (*Sync now*, *Settings*), the title NOTES, **Search** and **New note** as icons; under it one row with the view
  (*All*, *Favorites*, a category, ...) and the sort order, each with a drop-down arrow, and a toggle for the layout; then a
  count line ("12 NOTES") with the sync state and the number of changes not synced yet; then the notes with the title in
  bold, a quiet category and date line and dotted separators. The bordered text buttons (*New*, *Sync now*, *Settings*,
  *Quit*, *View:*, *Sort:*, *Clear*) are gone. Search opens from its icon and closes with its own control.
- Note list: new **Tile layout**. The toggle in the filter row shows the notes as cards (two to three to a row) with title,
  category, date and favorite star; the choice is remembered, also after a restart. Both layouts page by whole rows.
- The page-up and page-down buttons everywhere in the app are now two plain arrow icons.
- The app asks for the Roboto typeface (the one the reader's own apps use) when the reader has it, and logs which
  typeface it uses. No change to notes, sync or the data.

## 1.6.0

- Launcher: the PB Notes icon in the application menu no longer sits inside a heavy square frame. It is the notes
  page on its own, drawn as soft-edged line art of about the same size and weight as the built-in apps' icons; the
  pressed (focused) variant is the same picture inverted. Nothing else changes: no change to notes, settings,
  sync or the data.

## 1.5.0

- Attach: the file browser behind **Attach** in the editor now opens on a fixed list of folders — *Audio Books,
  Downloads, Music, My pictures, Notes, Photo* and *screens* (those that exist on the reader) — instead of the whole
  storage. The files at the top of the storage (such as `Autorun.inf` and `install.md`) and other top-level folders
  are no longer shown, and **Up** stops at this list. Inside these folders everything is listed as before.
- Attach: a button at the top of the file browser switches between the **List** and a **Tile** view (a grid with a
  folder or file symbol and the name, about twice as many entries per page). The choice is remembered, also after a
  restart. Both views page by whole rows and no row is cut off at the bottom of a page.
- Attach: in the Tile view, pictures (JPG, PNG, GIF, BMP, WebP) show a small grayscale **preview** of the picture.
  Previews are made only for the tiles on the page, one at a time in the background, so tapping, paging and changing
  folders never wait for them; a picture that cannot be read keeps its file symbol and can still be attached. Previews
  are kept in memory only (at most 8 MB) and nothing is written to the storage. Attaching itself, the size limit and
  the confirmation are unchanged and the original file is attached, never the preview. No change to the server, the
  data or the sync protocol.

## 1.4.0

- Settings: new section **Synchronization** with **Auto Sync** (on by default, remembered across restarts, works
  without an account). When it is on, saving or deleting a note, changing its category or favorite
  mark, or ticking a task box starts the ordinary sync by itself, about 4 seconds after the last change and at most once
  every 15 seconds, so changes made close together share one sync. Saving and the editor do not wait for the network.
  With Wi-Fi off, the reader asks the device to connect once (as *Sync now* does) and syncs when it is up. Without an
  account, or if the connection fails, nothing is attempted and no error is shown: the note stays marked as waiting and goes
  out with the next sync (a later save, *Sync now*, or the sync at app start). It never retries on a timer and starts no
  automatic sync while an editor is open. A failed sync shows its reason in the status line, as for
  *Sync now*. The **Sync now** button reads "Syncing..." (and is disabled) from the moment a save triggers an automatic
  sync, through the short wait and the Wi-Fi connection, until the sync ends. With the switch off, saving behaves exactly
  as before.
- Safety: if a sync replaced a note while you were editing it, saving keeps the server's version as the note and your text as
  a new note marked "(conflict ...)", and tells you once (the same notice as for a conflict found by a sync). Before, the older
  text would have been sent over the newer server version.
- No change to the server, the data format or the sync protocol; the preference is one new stored setting.

## 1.3.1

- Start: after some use the reader could refuse to start PB Notes (and InkFeed) until it was restarted: the log showed
  `failed to open device file /dev/mali0 ... Permission denied` and exit code 134. The cause is in the reader's system, not
  in the app, and is not fixed by this release. The launcher now keeps the log of the launch before as `pbnotes.log.prev`,
  records the user, the GPU device, uptime, memory and running app processes at every start, keeps the app's start-up lines
  when it aborts, and retries once after exactly this refusal. If the retry does not help, restart the reader. Nothing
  changes in the app, the data or the sync.

## 1.3.0

- Reading page: **fenced code blocks** (backtick or tilde fences, optional language such as `yaml`) are shown as one boxed
  area in fixed-width type with every line and space as written, long lines wrapped and no line cut by a page break;
  nothing inside is formatted and no link in it can be followed. **Headings** `#` to `######` get clearly different sizes
  and a heading is never left alone at the bottom of a page. New formats: `~~strikethrough~~`, `<u>underline</u>`,
  **quotes** (`> ` with a bar at the left), **info boxes** (`> [!NOTE]`, `[!TIP]`, `[!WARNING]`, `[!IMPORTANT]`,
  `[!CAUTION]`, labelled in words) and collapsible **`<details>` sections** that open and close by a tap on their summary.
  Copying from the reading page returns the note's own text with these marks.
- Editor: typing **`/`** at the start of a line or after a space opens a list of twelve formats (bullet list, table, info
  box, link, bold, italic, underline, strikethrough, details, quote, code block, emoji) next to the caret; choosing one
  inserts its Markdown with the caret where typing continues. `//` opens a **date-time picker** (starts at now, writes
  `YYYY-MM-DD HH:MM`) and *Emoji* a grid of 48 emojis. Each insertion is one Undo step; Back closes only an open popup.
- A fence line, a task box or an attachment reference inside a code block of either kind is treated the same everywhere
  (reader, attachments, task toggling, tables). Notes are stored and synced unchanged: no schema, setting or sync change.

## 1.2.0

- Editing tools in a context menu, like the reader's own: a long press on a word selects it and shows two draggable
  markers and a bubble with Cut, Copy, Paste, Remove, Select all, Undo and Redo (only those that apply); a long press
  on empty text offers Paste, Select all, Undo and Redo. The editor's header keeps only Cancel, Save, Favorite, Attach
  and Delete. Pasted text is cleaned (control characters dropped, line breaks in the title become spaces, at most
  1,000,000 characters).
- *Undo* and *Redo* over the title, the text, the category and the favorite marker in one history; undoing
  everything clears the unsaved-changes prompt. The history is never saved or synced.
- Reading page: the note's title is shown above the text (bold, larger, wrapped in full; none for a note without
  a title); a long press selects a word (or a table row, an attachment, a blank line), the markers extend the selection
  across blocks, and *Copy* puts the note's own source text, with the marks of completely selected elements, on the
  clipboard. The page stays read-only and its footer keeps only the page buttons.
- The clipboard is kept in the app and uses the system clipboard where the reader's platform provides one.

## 1.1.0

- Explicit note title: a separate Title field in the editor; the title is never guessed from the first line,
  survives text edits, is sent to the server with every write and is part of conflict detection (a
  title-only edit on both sides keeps both versions; a conflict copy carries the marker in its title).
- Category picker: choose an existing category, "No category" or "New category…" from a list instead of
  typing the name.
- The list preview now keeps a first line that is not the title.
- Local database upgraded to schema 3 (one way, with backup): unsynced notes keep the title they were listed
  with.

## 1.0.0

- First release. Notes with links, tables, emojis and attachments, synchronized with a Nextcloud Notes
  server (features 001 and 002).
- About screen: name, version, author, copyright, the app license and the licenses of all third-party
  components, readable offline (feature 003).
