# Changelog

One entry per release, newest first. The heading is the content of `VERSION`; the release is tagged
`v<version>` after the merge. `scripts/check-version-bump.sh` (part of `scripts/pre-merge.sh`) fails when a
branch changes the product without raising `VERSION` and adding the entry here.

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
