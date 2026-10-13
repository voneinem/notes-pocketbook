# Security review (host-testable core)

Date: 2026-10-10. Scope: the Qt-free core library (`core/`), its build and test tooling, and the
repository. The Qt UI and the device behavior are NOT covered yet: they depend on the manual device
gates (tasks T114, T118, T021, T042) and are listed under "Open items".

Method: every claim below is backed by an automated test or a repeatable command; the command or
test name is given. Run everything with `scripts/pre-merge.sh`.

## 1. TLS and credentials (constitution Principle III)

| Requirement | Evidence |
|-------------|----------|
| Certificates are always verified (peer and host name); no switch can turn this off | `core/src/curl_transport.cpp` sets `VERIFYPEER=1`, `VERIFYHOST=2`; `scripts/check-constitution.sh` (rule R1) fails the build on `curl -k`, `--insecure`, `--no-check-certificate`, and on `VERIFYPEER/VERIFYHOST/VERIFYSTATUS` (also the `PROXY_` and `DOH_` variants) set to 0/false or `VERIFYHOST` set to 1; its self-test proves each pattern is detected |
| A self-signed server certificate is refused and no credentials are sent | `test_tls_verification` (server received 0 requests, no `Authorization` header) |
| An expired certificate and a wrong host name are refused | `test_tls_cert_failures` (both signed by a CA the client trusts) |
| Credentials are not forwarded to another host/port on a redirect; a redirect to `http://` is refused | `test_redirect_credentials` (the redirect target saw no `Authorization` header and no connection) |
| Only `https://` URLs are accepted, also for redirects; credentials never in URLs | `test_connection_test` (URL normalization), `test_tls_verification` (plain `http://` refused before any connection), `CURLOPT_PROTOCOLS_STR`/`REDIR_PROTOCOLS_STR` = `https` |
| Trust anchors: device store when usable, else the bundled CA file; a user-supplied `custom-ca.pem` only extends them | `test_ca_selector`, `test_effective_ca`, `test_private_ca` (a private CA works only for the CA in the file; a different CA does not help; a wrong host name is still refused with the custom CA loaded; a self-signed server certificate placed in the file is accepted) |
| An invalid custom CA file is ignored, never weakens verification | `test_effective_ca` (empty, unreadable, over 1 MB, non-PEM, malformed base64, not DER, unterminated) |

Residual points: (a) libcurl's compiled-in default CA directory is not disabled; on the device this
is the SDK's `/etc/ssl/certs`, whose real content is unknown (open item O1). (b) Minimum TLS version
is 1.2.

## 2. Secrets and personal data

| Requirement | Evidence |
|-------------|----------|
| No secrets or real user data in the repository | `scripts/pre-merge.sh --secrets` (built-in patterns for private keys, Basic auth values, hard-coded password/token assignments; `gitleaks` is used instead when installed); fixtures are checked by `scripts/check-fixtures.sh` (only `example.com`/`example.org`/`localhost` hosts, `@example.com` addresses, no `Authorization` values) |
| No absolute local paths in tracked files | `scripts/check-constitution.sh` rule R5. Older commits still contain such paths in their history (see O4) |
| Logs contain no passwords, user names, note text or titles | `test_log` (name-based redaction, newline injection), rule R4 of `check-constitution.sh`, and `test_account_store` (the password never reaches the logger) |
| "Forget account" leaves no trace of the password | `test_account_store`: after forgetting or changing the password, the old password string is found nowhere in the database file, its journal, or `pbnotes.db.bak` (`secure_delete=ON`, the backup is removed after a successful migration) |
| The password is stored unencrypted | By design and documented in `README.md` and (for the UI) the setup screen text. Anyone with access to the device's USB storage can read it; use an app password. No OS keystore exists on the device (to be confirmed on the device, O2) |

## 3. Input validation

- Server responses: `core/src/json.cpp` is a strict whole-document parser (`test_json`: malformed
  documents, lone surrogates, control characters, nesting depth); the API client rejects wrong types
  and never returns partial results (`test_api_list`, `test_api_capabilities`).
- Imports/files: the custom CA file is size-limited and structurally validated (section 1).
- User input: URL normalization rejects credentials, queries, fragments, spaces and non-https
  schemes; categories are validated (length, spaces, control characters, UTF-8) and rejected
  values never replace the stored one (`test_category_validation`).
- SQL: all statements are prepared with bound parameters; search is done in application code, so
  `%`, `_` and quotes are literal (`test_store_search`).

## 4. Data safety (constitution Principle VI)

- Writes send `If-Match` (`test_api_write`); deletes check first with a conditional GET and never
  delete a note that changed on the server (`test_api_delete`, `test_sync_delete_edit`).
- A conflict keeps both versions: the server version stays the original, the local text becomes an
  uploaded conflict copy (`test_sync_conflict`). The resolution is one transaction that inserts and
  verifies the copy before replacing the original; a failure injected at every statement leaves the
  complete before or after state (`test_conflict_atomic`).
- A dropped request or a lost response at every point of a sync loses nothing and corrupts nothing
  (`test_sync_robustness`); a note edited while its push is in flight stays pending.
- Known limitation: if the server creates a note but the response never arrives, the next sync
  creates it again, so it can exist twice with identical text (spec FR-029, research R-8).

## 5. Findings during implementation

| # | Finding | Resolution |
|---|---------|------------|
| 1 | The first TLS test run recorded a zero-byte "request" for the refused self-signed case | A test-harness artifact (TLS 1.3 lets the server finish before the client's rejection arrives); the harness now counts it as an aborted handshake. No request was ever sent |
| 2 | The spec said the conflict copy equals the local text byte-for-byte while the title gets a conflict marker | Reconciled: the copy equals the local text except for the marker on its title line; wording fixed in spec artifacts and tasks |
| 3 | The fake server's note ids started at 100 while tests assumed 1 | Test fixture fixed |

## 6. Open items

- **O1** Device CA store, Qt runtime version and Wi-Fi API semantics: run `scripts/probe-device.sh` and
  the Hello World gate on the InkPad (T021, T114).
- **O2** Whether the device offers any keystore or restricted file permissions for the database.
- **O3** Behavior of a real Nextcloud server for `If-Match` on `PUT`/`DELETE`, title derivation and
  `pruneBefore` (T022); until then fixtures are marked `.unverified.json`.
- **O4** Commits made before the path clean-up contain local absolute paths in their history. Rewrite
  the history before the first public push if that matters.
- **O5** `gitleaks` is not installed on the development machine; the built-in scan is weaker.
- **O6** The Qt UI (bridge, QML pages, device build of the full app) has not been implemented or
  reviewed yet.

## 5. Rich note content (feature 002: links, tables, emojis, attachments)

Date: 2026-10-11. Scope: the new Qt-free core code, the bridge and the device glue. Everything below that says
"device" is not yet verified on the reader (tasks T018, T027, T036, T061, T077, T083).

| Concern | What was checked | Evidence |
|---------|------------------|----------|
| Attachment references from notes and from the server are hostile input | A reference is used only if, after percent-decoding each segment, it is non-empty, at most 1,024 bytes, has no NUL, control character or backslash, no scheme, no leading `/` and a non-empty file name; a bad or truncated escape and a decoded `/` inside a segment fail. The server's `filename` answer goes through the same validation | `test_attachment_ref`, `test_markdown_attachments`, `test_api_attachment_upload` (invalid answers are `rejected`) |
| No local path is built from a note or server string | Local files are `<row id>` or `<row id>.<[a-z0-9]{1,8}>` only; `record_downloaded` and `set_staged_file` refuse any other name; the image provider only opens such names; the janitor only deletes such names (and `*.part`) | `test_store_attachments` (`local_file_is_an_id_with_a_safe_extension_or_nothing`), `test_sync_attachments` (`the_remote_path_never_becomes_a_local_path`, the janitor test), `test_attachment_image_provider` |
| Links: only `http`/`https` are ever opened | `is_followable_url` (scheme, host, no blanks/control characters, 2,048 bytes) is applied in the parser and again in `DeviceBridge::openUrl` before `OpenBook`; the UI receives link indexes (`l<N>`), never raw addresses in markup | `test_links`, `test_markdown_links`, `test_reader_blocks` |
| The viewer hand-off cannot be pointed at other files | `DeviceBridge::openFile` accepts only a regular file whose canonical path is inside the attachment folder (a link that leaves it is rejected) and whose extension is on the allow-list | code review; device check T061 |
| Credentials on attachment requests | Same transport as notes: TLS verification always on, `https` only, credentials not forwarded to another host, no redirect to `http`; checked for downloads, HEAD and multipart uploads | `test_attachment_transport`, `test_attachment_upload_transport` |
| Size limits | 10 MiB for download (header and running total), add (before and while copying) and upload (before sending); a body over the limit leaves no file | `test_attachment_transport`, `test_api_attachments`, `test_attachment_service_add`, `test_api_attachment_upload` |
| Decompression bombs | An image of more than 40 million pixels is refused before decoding (Qt's own allocation limit applies as well); decoding is scaled and runs off the UI thread | `test_attachment_image_provider` |
| File browser previews (spec 009) read only what the browser listed | `image://filepreview/<id>` takes an integer id the latest folder listing handed out (regular, readable files with an image extension); the provider has no way to be given a path, an id of an earlier listing resolves to nothing, a file that turned into a pipe or device is not opened, and the 40 million pixel guard of the attachment decoder applies. Decoding is scaled, runs on one private worker thread, results are kept only in memory (8 MB, least recently used out first) and nothing is written to the card; log lines carry no name or path | `test_file_preview_provider`, `test_attachments_bridge` (`only_regular_readable_image_files_are_marked_as_images_and_given_a_preview_id`) |
| Hostile text makes the parser slow | A line longer than 16,384 bytes is shown as plain text, so many unmatched openers cannot make the inline parser quadratic | `test_perf_read` (`hostile_lines_are_parsed_in_linear_time`) |
| The user's own files | The add flow only reads the source (hash and modification time unchanged); the janitor never deletes a name this app does not create | `test_attachment_service_add`, `test_sync_attachments` |
| Logs | Only result words, counts and error classes: no address, file name or path; rule R6 of `check-constitution.sh` rejects log keys named like `url`, `path`, `file`, `name`, `address` (self-tested) | `check_constitution`, `check_constitution_selftest` |
| Data safety (Principle VI) | The note text is the source of truth; a file reaches the server before any reference to it; a failed or interrupted upload leaves the token in the local text and sends the text without it; the staged-to-pending binding and the token rewrite are single transactions (failure injection); a conflict copy keeps or duplicates its attachments | `test_store_attachments_staging`, `test_sync_attachments_push` |

Accepted risks and open items:

- The attachment folder is a normal folder on the device's FAT partition: no file permissions exist there, so
  anyone with USB access can read downloaded attachments (the same as the notes database).
- The file picker shows the whole storage: a user who deliberately attaches the app's own database would upload
  the stored app password into a note. This is a user action, not something the app does by itself.
- A decoder bug in the device's Qt image plugins cannot be ruled out; the formats the device supports are logged
  at start (`image.formats`) and unsupported ones fall back to a file entry.
- Not verified against a real server: the exact answers of the attachment endpoints (HEAD length, upload name,
  error statuses); the fixtures are `.unverified` until probe T005 is done.

