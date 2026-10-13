# Changelog

One entry per release, newest first. The heading is the content of `VERSION`; the release is tagged
`v<version>` after the merge. `scripts/check-version-bump.sh` (part of `scripts/pre-merge.sh`) fails when a
branch changes the product without raising `VERSION` and adding the entry here.

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
