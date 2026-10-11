# Quickstart: validating Rich Note Content

Validation guide only; implementation details live in `tasks.md`. Contracts:
[notes-attachments-api](contracts/notes-attachments-api.md), [core-library](contracts/core-library.md),
[qml-bridge](contracts/qml-bridge.md). Data: [data-model](data-model.md). Decisions and probes:
[research](research.md). Build, packaging and device prerequisites are those of
`specs/001-pocketbook-notes-mvp/quickstart.md` §1–§3.

## 1. Host build and tests (no Qt, no device)

```bash
cmake -S . -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host
ctest --test-dir build-host --output-on-failure
scripts/pre-merge.sh            # tests, constitution checks, fixture anonymization, secret scan
```

Expected: all suites pass offline. New test files (names are the intended ones):
`unit/test_utf8_cluster`, `unit/test_links`, `unit/test_markdown_links`, `unit/test_markdown_table`,
`unit/test_markdown_attachments`, `unit/test_paging`, `unit/test_attachment_ref`,
`unit/test_store_attachments`, `unit/test_store_attachments_staging`, `unit/test_attachment_service_add`,
`unit/test_store_migration_v2`, `unit/test_emoji_roundtrip`, `contract/test_api_attachments`,
`contract/test_api_attachment_upload`, `integration/test_sync_attachments`,
`integration/test_sync_attachments_push`, `integration/test_attachment_transport` and
`integration/test_attachment_upload_transport` (size cap, redirect, TLS),
`bridge/test_reader_blocks`, `bridge/test_attachment_image_provider`, `bridge/test_attachments_bridge`.
Each is written first and observed failing before its implementation (constitution I).

## 2. Test content (create on the Nextcloud test server)

Create one note with the Notes web app and paste:

```markdown
# Rich content check 😀

Plain address: https://example.org/path/page and a titled link [Read this](https://example.org/a?b=1),
a link whose text is its address [https://example.org](https://example.org), and a mail link
[write](mailto:someone@example.org) that must stay plain text.

| Item | Qty | Note |
|:-----|:---:|-----:|
| Tea | 2 | **loose** leaf |
| Rice | 10 | see [shop](https://example.org/shop) |
| A very long cell that has to wrap inside its column instead of running off the screen | 1 | ok |

Emojis: 👍 🇩🇪 👨‍👩‍👧 👍🏽 1️⃣ ❤️ 🫠
```

Then attach, with the web app's attach button, a PNG photo (about 1 MB), a JPEG, and a PDF (all
under 10 MiB), and one file over 10 MiB (to see the refusal). Keep an `.xyz` file as an unknown type.

## 3. Install and run the device scenarios

Build and install as in the MVP quickstart (the package now also carries
`applications/pbnotes-emoji.ttf` and `applications/pbnotes-emoji-OFL.txt`). Sync once.

1. **Links (US1)**: tap the plain address → browser opens at once (no prompt); return → same note
   and position. Tap *Read this* → prompt shows the real address; Cancel keeps the note, Open
   launches the browser. Tap the self-addressed link → opens at once. The mail link is plain text
   and cannot be tapped. With Wi-Fi off: the browser reports the page cannot load; the app is
   unaffected.
2. **Tables (US2)**: header bold with a rule, columns aligned left/center/right, the long cell
   wraps, emphasis and the link work inside cells, page down never cuts a row. Edit the note: the
   text is the original pipe text; save without changes → no `modified` change.
3. **Emojis (US3)**: all listed emojis show (monochrome); the list entry for *Rich content check*
   shows the title emoji; add a word in the editor on the device, save, sync, and compare the
   server text byte for byte with the original in the emoji line. Search for 😀. Note which sequences
   appear as parts and which unsupported emoji shows as a box.
4. **Attachments, view (US4)**: after the first sync nothing was downloaded (check the size of
   `…/pbnotes/attachments/`: empty). Open the note: placeholders with names, sizes after a moment.
   Download the PNG → shows in place; tap it → full screen fitted, Back returns to the same
   position. Download the PDF → *Open* hands it to the reader. Switch Wi-Fi off, reopen: both still
   work; the not-downloaded JPEG still shows its placeholder. Try to download the over-limit file →
   refused with a message, nothing stored.
5. **Attachments, add (US5)**: Wi-Fi off, edit a note, Attach, pick a file from the file browser →
   name and size confirm → token appears at the cursor → Save → note pending, file reads in the
   reader as *pending upload*. Leave a second edit with Discard → no staged file remains. Turn
   Wi-Fi on, sync → file and text appear on the server and in the web app (the image renders
   there), and the local text now holds the server path.
6. **Interrupted upload**: start a sync and switch Wi-Fi off midway → note stays pending with no
   text lost; sync again → completes. Repeat with the app killed mid-sync.
7. **Conflict with an attachment**: edit a note with a pending attachment on the device, edit the
   same note on the server, sync → server text stays, the conflict copy keeps the attachment.

## 4. Probes (run once; results go into research R-15)

| Probe | How |
|-------|-----|
| HEAD length | `curl -I -u user:apppw "https://server/index.php/apps/notes/api/v1.4/attachment/<id>?path=.attachments.<id>/name.png"` → is `Content-Length` there? |
| Empty create | `POST …/api/v1/notes` with `{"content":""}` → accepted? |
| Image formats | Start the app on the reader and read its log (supported formats line) |
| Emoji font | Open the test note: glyphs, sequences, missing-glyph box |
| Browser return, viewer | Scenarios 1 and 4 above |

## 5. Success criteria spot checks

- SC-001: count taps to open a plain link (1) and a titled link (2).
- SC-002: ten realistic tables from real notes read without sideways scrolling and without a cut row.
- SC-003: the emoji list above round-trips unchanged (diff the server text before and after).
- SC-004: after download and with Wi-Fi off, every downloaded attachment opens.
- SC-005: stopwatch for opening the mixed test note (< 3 s) with the host benchmark set of 1,000 notes.
- SC-006/SC-008: the interruption and rejection scenarios (host integration tests plus steps 6–7).
- SC-009: stopwatch for adding a file and saving offline (< 1 min).
