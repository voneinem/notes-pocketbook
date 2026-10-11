# Research: Rich Note Content (Links, Tables, Attachments, Emojis)

All items below resolve what the plan's Technical Context left open. Items marked **[DEVICE]** could not
be verified in this session because the reader was not attached; each has a probe task and a
fallback, and none blocks design. Sources are the public Nextcloud Notes repository (API
description, routes, web editor behavior — read for behavior only, nothing copied), the PocketBook
SDK headers in the local SDK, and `docs/` of this repository.

## R-1. How the Notes server stores and serves attachments

- **Findings** (Notes `docs/api/v1.md`, `appinfo/routes.php`, `NotesApiController`):
  - Download: `GET <server>/index.php/apps/notes/api/v1.4/attachment/{noteId}?path=<relative path>`
    returns the file body (404 if missing, 401 on bad credentials). The route accepts `v1` and
    `v1.4` as `apiVersion`; the documentation names `v1.4`.
  - Upload: `POST <server>/index.php/apps/notes/api/v1.4/attachment/{noteId}` with a multipart form
    field `file`; success is JSON `{"filename": ".attachments.<noteId>/<name>"}`. The name is the
    original name, deduplicated by the server, inside a per-note folder `.attachments.<noteId>`
    next to the note file (inside the note's category folder).
  - Notes apps older than the attachment API answer 400.
  - No size limit is enforced in the controller; PHP/web-server limits apply and surface as
    4xx/5xx on upload.
  - A note's text references an attachment by the **relative path**, percent-encoded per path
    segment: `![original name](.attachments.12/photo%20one.png)`. The web editor builds it this way;
    it also lets users insert existing files from elsewhere in the Notes folder, giving relative
    paths such as `../Images/a.png`. The web renderer treats a relative image target as an
    attachment and shows non-image extensions (anything but jpg, jpeg, bmp, webp, gif, png) as a
    download box. Ordinary links are not rewritten.
- **Decision**: Use exactly these two endpoints with `v1.4`, only when the stored API versions
  (already kept in the account row) include `1.4`. Without it, Download and Add show "the server
  does not support attachments" and everything else works. Recognize as an attachment:
  (a) an image reference `![alt](target)` whose target is a relative path (no scheme, not
  starting with `/`, no backslash or control characters after decoding, at most 1,024 characters);
  (b) a link `[name](target)` whose target is a relative path starting with `.attachments.`.
  Anything else that is not an `http`/`https` link stays plain text (spec FR-004). An image
  reference with an `http`/`https` target is **not** fetched (spec FR-005); it is shown as a link
  with its alt text.
- **Rationale**: matches what the web app and other clients write, so notes round-trip. Paths with
  `..` are allowed because the web client produces them and the server confines them to the notes
  folder; the device never uses the path to build a local file name (see R-5).
- **Alternatives**: WebDAV (`remote.php/dav/files/...`) — needs the notes folder name and
  per-user paths, more requests, no benefit; rejected. Inline `data:` images — not how Notes stores
  them.

## R-2. Learning an attachment's size without downloading it

- **Findings**: The note JSON has no attachment metadata. The only sources are response headers.
  The API route is a streamed response; whether `Content-Length` is present on `HEAD` is not
  documented. **[DEVICE/SERVER]** to be probed against a real server (task in `tasks.md`).
- **Decision**: Size is learned lazily, never by sync: when a note is opened with Wi-Fi up, a
  short-timeout `HEAD` (3 s) is sent once per attachment whose size is unknown, in a worker thread;
  the placeholder fills in the size when the answer arrives; the result is cached in the
  attachments table. If `HEAD` gives no length the placeholder keeps showing the name only
  (spec edge case "size is not known") and the size is learned from `Content-Length` when the
  user downloads. A `Content-Length` over the limit makes the download stop before the body.
- **Rationale**: keeps sync independent of attachments (spec FR-021, MVP SC-009), avoids a request
  storm on first sync of a large collection, works offline (name only).
- **Alternatives**: `HEAD` during sync — thousands of requests on a first sync; rejected. Ranged
  `GET` (`Range: bytes=0-0`) — PHP streamed responses ignore ranges; rejected as the primary way.

## R-3. Add-attachment flow and upload ordering (spec FR-025 to FR-031)

- **Problem**: a reference needs the server's real file path, which exists only after upload, and
  the upload URL needs the note's server id, which a never-synced note does not have. Other
  devices must never see a reference to a file that is not there yet (FR-028).
- **Decision**: a **local reference token** `![name](pbatt:<attachment id>)` is inserted into the
  text at the cursor when the user adds a file; the file is copied (never moved) into the app's
  attachment folder and registered as a *staged* attachment. Rules:
  1. Saving a note binds the staged attachments whose token is in the text to the note, in the
     same transaction as the save (so a crash cannot leave a saved token with an unbound file); staged
     attachments whose token is not in the text, and all staged attachments when the user discards
     or leaves, are deleted (the user's original file is never touched). Orphans left by a crash are
     purged at start.
  2. Push, per note, in this order: (a) if the note has no server id and has pending attachments,
     create it with the tokens **removed** from the text and store the id and ETag through the
     existing `confirm_push` (it keeps the note `edited` because the sent text differs from the
     local text); (b) upload each pending attachment (POST); after each success rewrite that
     token in the **stored** note text to the server's returned path (percent-encoded per
     segment) in one transaction that does not touch `modified` or the note status; (c) PUT the
     note (with `If-Match`) with its now fully resolved text. If an upload fails for a reason
     other than a transport abort (rejected, too large), the PUT is sent with that token removed
     from the **outgoing** text only; the local text keeps the token, the note stays pending and the
     attachment shows its reason.
  3. Transport-level failures (offline, TLS, auth, maintenance) abort the run as for notes; nothing
     is lost and the next run resumes at the first attachment still pending.
- **Rationale**: reuses the MVP push machinery (`confirm_push` already stores id/ETag but keeps
  the note pending when the sent text differs), satisfies FR-028 (the reference is only
  written to the server after the file exists there), and the local text rewrite makes retries
  idempotent. A lost response after an upload can leave an orphan file on the server; the retry
  uploads again and the server deduplicates the name. Accepted: same "safe duplicate" class as the
  MVP lost-POST case.
- **Alternatives**: uploading with a client-chosen path — the server decides the name; rejected.
  Creating the note first with the final text and fixing it later — would expose broken references
  to other devices; rejected.

## R-4. Conflicts with attachments (spec FR-030)

- **Decision**: when a conflict copy is created, attachment rows move with the **local** text.
  Pending (not yet uploaded) attachments are re-bound to the copy; a *downloaded* attachment
  referenced by a server path is duplicated for the copy as a new pending attachment (file copied,
  token substituted in the copy's text), so the copy is self-contained; a reference to an attachment
  that is not downloaded stays a path (it resolves on the server only while the copy lives in the
  same category folder — known limitation, shown as a placeholder with the reason if not).
- **Rationale**: both versions stay complete in the common case at the cost of a local file copy.
- **Alternatives**: always copy on the server side — no API for it; rejected.

## R-5. On-device storage and limits

- **Decision**: files live in `<data dir>/attachments/<attachment id>.<ext>`; `<ext>` is the
  original extension lower-cased and restricted to `[a-z0-9]{1,8}` (empty otherwise), so the
  PocketBook can pick a viewer from it and no remote string ever becomes a path. Metadata
  (display name, remote path, size, state, reason, note) is in a new `attachments` table (schema
  version 2). Writes go to `<id>.part` and are renamed after a size check. The size limit is
  **10 MiB** per file for download and add, one constant. Filesystem is vfat: no permissions, 4 GiB
  file cap (irrelevant under the limit), so no special handling beyond the existing transactional
  approach.
- **Rationale**: 10 MiB covers phone photos and typical PDFs yet bounds memory (a 12-megapixel JPEG
  is decoded scaled, see R-7). The spec leaves the value to planning.
- **Alternatives**: no limit — unbounded storage and memory; per-user setting — YAGNI.

## R-6. Transport changes for files

- **Findings**: `HttpTransport::perform` returns the whole body as a string and cannot upload files.
- **Decision**: extend `Request` with optional `upload_file` (path, form field `file`, file name),
  `save_body_to` (destination path) and `max_body_bytes`; add `HEAD` to the allowed methods. The
  curl implementation uses `curl_mime` for upload and a write callback to the file for download, and
  aborts as soon as `Content-Length` or the running total exceeds `max_body_bytes`. Credentials,
  TLS verification, protocol restriction and redirect rules stay exactly as for notes (the existing
  redirect and TLS tests are extended to attachment URLs). The fake transport/server in tests gets
  matching hooks.
- **Rationale**: smallest change that keeps the mockable seam (constitution V) and avoids holding
  10 MiB strings.
- **Alternatives**: a second transport for files — duplicates TLS/CA/redirect code; rejected.

## R-7. Showing images and full-screen view (spec FR-019, FR-031)

- **Decision**: a Qt image provider (`image://attachment/<id>?w=<pixels>`) decodes with
  `QImageReader` using scaled decoding (JPEG decodes at reduced size), converts to 8-bit grayscale,
  and returns an image at the requested width; the reader requests the note width, the full-screen
  page requests the screen size. Decoding is asynchronous so paging is not blocked. Images the
  decoder cannot read show the placeholder with the reason "format not supported". A new page
  `ImagePage` shows the image fitted (aspect preserved) with a Back control; no zoom or pan.
  Rotation is handled by the system exactly as for other pages (spec/MVP R-10).
- **[DEVICE]** Which `imageformats` plugins the device Qt ships (JPEG, GIF, WebP) is unknown; PNG
  is built into Qt. The app logs `QImageReader::supportedImageFormats()` at start (no personal data)
  and the probe task reads it. Unsupported formats fall back to the "file" entry.
- **Rationale**: no new dependency; grayscale conversion gives predictable e-ink output.
- **Alternatives**: decoding libraries (stb_image) — new dependency (constitution II), only worth it if
  the probe shows plugins missing; recorded as the contingency.

## R-8. Opening non-image files and web links on the device

- **Findings**: InkFeed opens the system browser with `OpenBook(BROWSER_FOR_AUTH, url, 0)` because
  `QDesktopServices` is a no-op on the device (`docs/inkfeed-analysis.md`, InkFeed dev notes);
  `OpenBook(path, params, flags)` is also how files are handed to the reader application, which
  picks a viewer by file type.
- **Decision**: `DeviceBridge` gets `openUrl(url)` and `openFile(path)`. `openUrl` re-validates the
  scheme (`http`/`https`), rejects whitespace/control characters and lengths over 2,048, and calls
  `OpenBook(BROWSER_FOR_AUTH, url, 0)` on the device and `QDesktopServices::openUrl` on the host.
  `openFile` allows only files inside the attachment folder whose extension is in a fixed list the
  reader application handles (pdf, epub, fb2, txt, rtf, doc, docx, html, htm, djvu, mobi, cbz, cbr);
  other types show "no viewer on this device" without calling the system. The InkFeed one-line
  pattern is adapted and already covered by the existing `NOTICE` entry (device glue).
- **[DEVICE]** Whether the app is kept alive and shows the same note on return after the browser
  closes is expected (InkFeed does the same) but must be checked on the reader; fallback would be
  to persist note id and reading position before launching (not built unless needed).
- **Alternatives**: our own browser view — large, rejected (spec assumption).

## R-9. Link parsing and the confirmation rule (spec FR-001 to FR-006)

- **Decision**: extend the Qt-free inline parser: `[text](url)` keeps `url`; bare `http://` and
  `https://` addresses and `<http://…>` autolinks are detected; trailing `. , ; : ! ? )` are not
  part of a bare address unless balanced; only `http`/`https` URLs with a non-empty host and no
  whitespace/control characters (at most 2,048 characters) become links; everything else renders as
  its label (as today). A new pure function `link_needs_confirmation(text, url)` implements spec
  FR-002: no confirmation iff the text is itself a web address naming the same destination, after
  ignoring the scheme, a leading `www.`, host letter case and one trailing `/`. The UI gets each
  block's links as a list and `Text.onLinkActivated` passes an index (`l<N>`), never the raw
  address, so no note-controlled string is interpreted as markup.
- **Rationale**: pure, host-testable, no regex dependency; the rule is exactly the clarified spec.
- **Alternatives**: Qt's `Text.MarkdownText` — no source mapping, not host-testable (MVP R-9).

## R-10. Markdown tables (spec FR-007 to FR-013)

- **Decision**: a table is recognized at a header line containing at least one unescaped `|`,
  followed by a delimiter line (`---`, `:--`, `--:`, `:-:` cells, same cell count as the header), at
  the top level (not in a list item, quote or code fence — those stay plain text). Rows continue
  until a blank line or a line without an unescaped `|`. Cells are split on unescaped `|`
  (`\|` is a literal pipe), leading/trailing pipes optional, cell text trimmed and parsed with the
  inline parser (emphasis, links, emojis). The column count is the largest row length; shorter rows
  are padded with empty cells and the header is padded, so no content is hidden. A malformed table
  is never partially drawn: if the delimiter line is invalid the lines stay ordinary paragraphs.
- **Layout**: the core returns, per column, a weight (clamped content length) so the UI can
  distribute the width with a minimum per column and wrap text; the table is flattened into one
  reader block per row, so page breaks never fall inside a row (R-11), and the header row is bold
  with a heavier rule below it.
- **Rationale**: Notes' web editor writes GitHub-style pipe tables (spec assumption); a small
  deterministic parser needs no dependency (constitution II).
- **Alternatives**: cmark-gfm — new dependency, no benefit over ~150 lines (MVP R-9).

## R-11. Paging without cutting rows or images

- **Decision**: the reader marks unsplittable blocks (table rows, images, attachment entries). A pure
  function in the core (`page_target`, host-tested) computes the next scroll position as before, but when an unsplittable block straddles
  the bottom edge of the page, the next page starts at that block's top (and symmetrically going
  back). A block taller than the view falls back to the plain page step so paging always
  progresses. Tapping edge zones uses the same function.
- **Rationale**: one small change in `PageButtons.qml`/`ReaderPage.qml`, no new component.

## R-12. Emojis: preserve, draw, and never cut

- **Preservation findings**: the JSON reader already decodes `😀` surrogate pairs (PHP
  servers escape non-ASCII this way) and rejects lone surrogates; the writer emits raw UTF-8; SQLite
  stores bytes; search folds with code-point-wise `fold_case` that leaves emojis untouched. There
  are no tests for 4-byte characters yet — tests come first (constitution I).
- **Cutting finding**: `make_excerpt` counts code points and can stop in the middle of a
  multi-part emoji (ZWJ sequence, flag, skin tone, keycap). UI elision (`Text.elide`) and the editor
  caret use Qt's grapheme-cluster rules, so they are fine.
- **Drawing decision [DEVICE]**: ship the monochrome **Noto Emoji** font (SIL OFL 1.1; file
  `ofl/notoemoji/NotoEmoji[wght].ttf` in the Google Fonts repository, pinned by commit and
  SHA-256 in `third_party/noto-emoji/VERSION` with `OFL.txt`; attributed in `NOTICE`), installed
  next to the app as `applications/pbnotes-emoji.ttf` (with `pbnotes-emoji-OFL.txt`, the same pattern
  as `pbnotes-cacert.pem`), loaded at start with `QFontDatabase::addApplicationFont`, and appended to the
  application font's family list so every `Text`/`TextArea` falls back to it per glyph. Reason: the
  device's own UI font renders only some symbols (InkFeed's notes list the ones that work and name
  one that does not; emoji coverage is unverified) and the screen is monochrome, so a monochrome font is the right match.
  The font file is read from disk at start, not compiled into `rcc` (the SDK `rcc` is unusable,
  the font is about 2 MB).
- **Stand-in decision**: no custom coverage detection. A character the font lacks is drawn as the
  font's missing-glyph box, which is the clear stand-in the spec asks for, and the text is
  untouched. The device probe checks that the box is visible; only if it is not, a small
  replacement in the *display* string (never the stored text) is added.
- **Cluster rule**: a small Qt-free helper (`utf8_cluster_end`) treats these as one unit: a code point
  plus following variation selectors, skin-tone modifiers, combining marks and keycap; `ZWJ` joined
  parts; pairs of regional indicators; tag sequences. It is used by `make_excerpt` so a cut never
  falls inside a unit (the excerpt may run a few characters over its limit).
- **Known limit**: how a ZWJ/flag sequence is drawn depends on the font's ligatures; where the font
  has none the parts appear side by side. They are still never split across lines or cut by
  shortening, which is what the data and layout rules guarantee.
- **Alternatives**: color emoji font — a monochrome panel gains nothing and the file is ~10 MB;
  relying on the device font only — unverified coverage (kept as the fallback if the font fails to
  load); a coverage-detecting replacement layer — complexity without proven need (constitution II).

## R-13. Choosing a file to attach

- **Decision**: a simple in-app browser page, `FilePickerPage`, lists a directory through a bridge
  call `listDirectory(path)` (name, is-directory, size; directories first, sorted; hidden entries
  skipped), starting at `/mnt/ext1` on the device and the home directory on the host. A tap on a
  directory opens it, a tap on a file shows the name and size in a confirm dialog (spec FR-025).
  Files over the limit are refused before the dialog's confirm.
- **Rationale**: no dependency on a QML module whose presence on the device is unverified
  (`Qt.labs.folderlistmodel` exists in the SDK but the device runtime set is not confirmed); about
  80 lines of QML and one `QDir` call.
- **Alternatives**: `QtQuick.Dialogs`/`Qt.labs.platform` — need a platform theme the reader lacks.

## R-14. Test strategy additions (constitution I, IV, V)

- Unit (host, Qt-free): inline links and autolinks, `link_needs_confirmation`, table parsing and
  column weights, attachment reference parsing/validation, token rewrite/strip, cluster helper,
  excerpt with emojis, JSON/Store/Sync round trip of 4-byte characters and surrogate escapes,
  schema migration 1 to 2 (including the backup rule), attachment store operations and cascades.
- Contract: attachment download/upload/HEAD responses against fixtures, including 400 (old server),
  401, 404, 413 and an upload response with a deduplicated name.
- Integration (fake server): add → pending → push order (create, upload, rewrite, PUT), interrupted
  upload resume, rejected upload, conflict with pending and downloaded attachments, size cap abort,
  download resumes nothing half-written, credentials never sent to a redirect target for
  attachment URLs, attachment download over a certificate failure refused.
- Bridge (offscreen Qt): reader block model for links/tables/attachments, image provider output,
  token insertion at the cursor, staged-attachment purge.
- Device (quickstart, manual): browser round trip, emoji drawing and missing-glyph box, image
  formats, picker, viewer hand-off.

## R-15. Probes to run on the reader and server (tasks)

| Probe | Question | If the answer is bad |
|-------|----------|----------------------|
| Server HEAD | Does `HEAD …/attachment/{id}?path=` return `Content-Length`? | Show names only; size learned at download |
| Server empty create | Does `POST /notes` accept empty content (needed to create a note whose text only had tokens)? | Create with a single placeholder line and replace it in the PUT |
| Image formats | Which of JPEG/GIF/BMP/WebP load on the device? | Mark unsupported formats as files; consider a decoder only if JPEG fails |
| Emoji font | Does the font load, do ZWJ/flag sequences shape, is the missing-glyph box visible? | See R-12 fallbacks |
| Browser return | Does the app keep its state when the browser closes? | Persist note id and position before launch |
| Viewer hand-off | Does `OpenBook` open a PDF copied to the attachment folder? | Narrow the extension list |
