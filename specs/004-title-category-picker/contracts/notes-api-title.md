# Contract: explicit title on note writes

Amends `specs/001-pocketbook-notes-mvp/contracts/notes-api-v1.md` (which stated "Title is never sent").

| Request | Body (JSON) | Notes |
|---------|-------------|-------|
| `POST notes` | `{title, content, category, favorite, modified}` | `title` is the stored, validated title of the note |
| `PUT notes/{id}` with `If-Match: "<etag>"` | `{title, content, category, favorite, modified}` | sent on **every** update, also when only category or favorite changed, so the server never re-derives a title from the text |

Answers (`200` + note object) are handled as before. The note's `title` in the answer is stored
(`title_for`): if it differs from what was sent (the server made it unique or replaced characters it
cannot use in a file name), the reader sees the server's title from then on.

Unchanged: `If-Match` on PUT, `412` → conflict handling (now title-aware, see
[core-library.md](core-library.md)), conditional GET before DELETE, TLS verification.

**[VERIFY]** (quickstart §3): an explicit title is honored on `POST`/`PUT`; duplicates and forbidden
characters are adjusted and reported back. Fixtures for the contract tests are anonymized and named
`*.unverified.json` until checked against a real server, as for the existing fixtures.
