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
