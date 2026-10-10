# Contract: Nextcloud Notes API v1 (as consumed by the client)

Behavior reference: `docs/notes-android-features.md` §5-6 and the public Notes API. Items marked
**VERIFY** are assumptions to be confirmed against a live server when recording fixtures
(`tests/fixtures/`). The client only talks to `https://` servers with a verified certificate.

## Common

- Base URL: `<server_url>/index.php/apps/notes/api/v1/` (notes), `<server_url>/ocs/v2.php/cloud/`
  (capabilities).
- Auth: HTTP Basic (`username:app password`) on every request. Credentials are never put in URLs.
- Headers sent: `Accept: application/json`, `User-Agent: pbnotes/<version>`,
  `OCS-APIRequest: true` (OCS endpoints), `Content-Type: application/json` (bodies).
- Timestamps: integer epoch **seconds** in both directions.
- Note JSON fields used: `id`, `etag`, `readonly`, `title`, `content`, `category`, `favorite`,
  `modified`. Unknown fields are ignored; missing optional fields get defaults; wrong types make
  the response invalid (reported as a server error, no partial write).

## Operations

| Operation | Request | Success | Handling of other statuses |
|-----------|---------|---------|----------------------------|
| Capabilities | `GET /ocs/v2.php/cloud/capabilities?format=json` | 200; `ocs.data.capabilities.notes.api_version` array contains a `1.x` entry | 401/403 → credentials rejected; missing notes section or no `1.x` → "server not supported" |
| List | `GET notes?pruneBefore=<s>` + `If-None-Match: <collection etag>` | 200 + JSON array (unchanged notes in pruned form with only `id`); response headers `ETag`, `Last-Modified`, `X-Notes-API-Versions` | 304 → nothing changed; 503 → maintenance; 401/403 → rejected |
| List (first pull) | `GET notes` (add `chunkSize=100` / `chunkCursor` if supported, **VERIFY**) | 200 + array | as above |
| Get one | `GET notes/{id}` (+ `If-None-Match: <etag>` for the delete re-check) | 200 + note; 304 unchanged | 404 → note gone |
| Create | `POST notes` body `{content, category, favorite, modified}` | 200 + note with `id`, `etag`, `title`, `modified` | 4xx/5xx → note stays `edited` |
| Update | `PUT notes/{id}` + `If-Match: "<etag>"`, body as create | 200 + note | **412** → conflict (client fetches server version, see sync spec); **404** → server deleted it: client re-creates with POST; other → stay `edited` |
| Delete | First `GET notes/{id}` with `If-None-Match: <stored etag>`; only on `304` then `DELETE notes/{id}`. If the server is verified to return 412 for a stale `If-Match` on `DELETE` (**VERIFY**), send `DELETE` with `If-Match` and skip the `GET` | 200 | GET `200` (changed) → conflict, no `DELETE` sent, server version restored; GET/DELETE `404` → treated as already gone; `DELETE` 412 → conflict |

Notes on the ETag format: the client stores the value exactly as returned (including quotes if the
server sends them) and sends it back unchanged in `If-Match` / `If-None-Match`.

## Client expectations (tested with fixtures)

1. Title is never sent; after a push the response's title replaces the local one.
2. A `412` on `PUT` triggers exactly: one `GET notes/{id}`, replacement of the original with the
   server version, creation of one conflict copy, one `POST` for the copy.
3. `304` on list leaves all notes untouched and stores nothing new except the sync time.
4. Server version list is stored; if no `1.x`, no note request is made.
5. Error bodies are never shown verbatim; the user sees a classified message
   (rejected credentials, unreachable, certificate, unsupported server, maintenance, server error).
6. No request is sent without a verified TLS channel (the transport refuses non-https URLs).

## Error classification (transport → UI message)

| Condition | Class | Message key |
|-----------|-------|-------------|
| DNS/connect/timeout | `network` | "Server not reachable" / status `offline` if no Wi-Fi at start |
| TLS verification failure | `tls` | "Certificate could not be verified" |
| 401/403 | `auth` | "Credentials rejected" |
| 503 | `maintenance` | "Server in maintenance, try later" |
| Capabilities lacks API 1.x | `unsupported` | "This server does not support Notes API v1" |
| other 4xx/5xx, invalid JSON | `server` | "Unexpected server response" |
