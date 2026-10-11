# Changelog

One entry per release, newest first. The heading is the content of `VERSION`; the release is tagged
`v<version>` after the merge. `scripts/check-version-bump.sh` (part of `scripts/pre-merge.sh`) fails when a
branch changes the product without raising `VERSION` and adding the entry here.

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
