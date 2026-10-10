# Nextcloud Notes Android app: feature and sync analysis

> **Licensing note.** The analyzed project (nextcloud/notes-android) is GPL-3.0-or-later. This document
> paraphrases observed behavior and API usage in my own words only. No source code, comments,
> documentation text or UI strings were copied. Endpoint paths and JSON field names of the public
> Nextcloud Notes / OCS APIs are mentioned as facts about those public APIs.
>
> Analyzed checkout: `<projects>/notes-android` (read-only), HEAD `f3eb0a1a`,
> Room DB schema version 30, minSdk 28, targetSdk 36. Code root:
> `app/src/main/java/it/niedermann/owncloud/notes/`. Statements marked **(unverified)** or **(inferred)**
> were not confirmed directly in the client code.

---

## 1. Overview

The app is a native Android client for the Nextcloud **Notes** server app. It keeps a complete local copy
of every note in a Room/SQLite database (so it is offline-first), pushes local edits to the server, and
pulls remote changes. All HTTP traffic goes through the Nextcloud **Single Sign-On (SSO)** library, which
delegates transport and credentials to the installed Nextcloud Files Android app. The app itself never sees
the user's password or app token.

Main technology stack (relevant to understanding behavior, not to be copied):
Room (DB), Retrofit over the SSO transport (REST), Gson (JSON), RxJava 2 (some API calls), WorkManager
(background jobs), LiveData/ViewModel (UI state), Glide (avatars/images), a markdown component from the
author's "nextcloud-commons" library for rendering/editing, and Nextcloud's android-common UI/theming libs.

---

## 2. Feature list (user visible)

**Notes**
- Create, edit, delete notes; plain text storing markdown. Title is a separate field on API v1, derived from
  the first non-empty line of content (markdown stripped) when the server/API does not supply one.
- Categories (a single string per note); a slash in the category name acts as a folder separator, giving a
  two-level-plus hierarchy in the navigation drawer (top-level entries aggregate counts of sub-categories and
  can be expanded).
- Favorites (star toggle).
- Excerpt shown in list (first ~200 chars of content after the title, markdown removed, checkboxes turned
  into symbols, newlines replaced by a visual separator; HTML-looking content is tag-stripped instead).
- Note modes: plain **edit**, rendered **preview** (with tappable checkboxes that write back into the
  text), and an optional **direct edit** (server-side rich text editor in a WebView, see section 8).
- Per-note remembered scroll position and last-used mode.
- In-note search with match counter, next/previous navigation and highlighting.
- Internal links: a link whose target is a plain number is interpreted as the server id of another note
  in the same account and opens that note.
- Formatting help screen (markdown cheat sheet).
- Move a note to another account (implemented as delete in the old account plus create in the new one).
- Share note text via the Android share sheet; pin note to home screen as launcher shortcut; recent notes are
  also published as dynamic launcher shortcuts after each sync.
- Read-only viewing of external text files opened via a VIEW intent (not stored).

**List / navigation**
- Drawer entries: All notes, Favorites, Uncategorized (hidden when empty **(inferred)**), then categories with note counts.
- Sorting per view: last modified (descending), title ascending, title descending; the toggle cycles through
  the three. Choice is stored per category (DB table) and, for the three virtual views, in shared preferences
  (not account specific; the code carries a TODO about that).
- Favorites are pinned to the top in "All notes", "Uncategorized" and category lists (not in the Favorites
  view itself).
- Section headers: by time bucket (today, yesterday, this/last week, this/last month, month+year, year) when
  sorted by date; by first letter (A-Z, accented Latin, "#" and "other") when sorted by title; by category when
  viewing a category (shows sub-category headers).
- List or staggered grid layout (preference); swipe left/right to delete or toggle favorite (preference,
  disabled in grid); multi-select action mode with bulk delete, bulk set category, bulk share/copy, move to
  another account.
- Search bar over the current view, filtering by title or content (SQL `LIKE %term%`), with highlighting.
  A new note created while a search query is active pre-fills the query as content.
- Pull-to-refresh triggers capabilities refresh plus full sync.

**Accounts / server**
- Multiple accounts, switcher dialog with avatar (fetched from `/index.php/avatar/<user>/64`), manage-accounts screen
  (remove account with warning about unsynced notes, set server-side notes folder path and file suffix via
  Notes settings API).
- Nextcloud user status (online status, status message) editing from the account switcher.
- Link to trash bin (opens Files app trash bin, falls back to web UI), ecosystem app banner in drawer (Files, Talk).

**Settings** (one preferences screen): sync only on Wi-Fi; periodic background sync; theme (light/dark/system);
show ecosystem apps; default note mode for opening notes; enable direct edit; grid view; swipe actions;
monospace font; font size (small/medium/large); keep screen on; app lock; block screenshots/recents preview.

**Other**: About screen (credits/license/contributing tabs), crash/exception dialog with troubleshooting tips
and shareable report, many translations, RTL support.

---

## 3. Screens (activities / fragments)

| Screen | Package / class | Purpose |
|---|---|---|
| Main (note list) | `main/MainActivity`, `MainViewModel` | Drawer + search bar + list/grid + FAB for new note, selection action mode, pull-to-refresh |
| Import account | `importaccount/ImportAccountActivity` | First-run welcome; starts SSO account picker; shows import progress |
| Edit note | `edit/EditNoteActivity` + `NoteEditFragment`, `NotePreviewFragment`, `NoteDirectEditFragment`, `NoteReadonlyFragment` | Hosts one fragment per mode; toolbar shows title and category; menu: mode switch, favorite, category, title, move, share, delete, cancel, pin |
| Category dialog | `edit/category/CategoryDialogFragment` | Type-ahead search over existing categories, or enter a new one, or clear |
| Edit title dialog | `edit/title/EditTitleDialogFragment` | Rename (only offered if API >= 1.0) |
| Account picker dialog | `accountpicker/` | Pick target account for "move note" |
| Account switcher | `accountswitcher/` | Switch/add/manage accounts, user status bottom sheets |
| Manage accounts | `manageaccounts/` | Delete account, edit notes path, edit file suffix |
| Preferences | `preferences/` | See section 2 |
| Formatting help | `FormattingHelpActivity` | Markdown cheat sheet |
| About | `about/` | Credits, license, contributing |
| Share (Nextcloud sharing) | `share/NoteShareActivity`, `NoteShareDetailActivity` | Manage Nextcloud shares of the note file (see section 8) |
| Append to note | `AppendToNoteActivity` | Reuses the main list so the user can pick which note to append shared text to |
| Exception | `exception/` | Error report screen |
| Widget config screens | `widget/*/…ConfigurationActivity` | Choose account and note/filter for a widget |

All activities except a few derive from a base class that applies server-derived branding colors and, for
the app lock, gates access (section 9).

---

## 4. Data model (conceptual)

Room schema v30 (`app/schemas/.../30.json`, migrations from v9 onward exist in `persistence/migration/`;
the migration chain itself is not interesting for a port).

### 4.1 Note
Per-account rows. Fields:
- local id (autoincrement, primary key) and **remote id** (nullable; null = never synced / server id unknown).
- account id (foreign key to Account, cascade delete).
- **status**: one of *void* (in sync), *local-edited* (created or changed locally, pending push),
  *local-deleted* (deleted locally, pending push; hidden in UI, row kept until server deletion succeeds).
- title, category (empty string = uncategorized), content, favorite flag.
- modified time (stored as epoch millis locally; the JSON API uses epoch **seconds**).
- ETag string from the server (per note).
- excerpt (derived locally, never sent).
- read-only flag and shared flag (reported by the server, `readonly` and a share indicator; read-only is stored but I did not find UI logic that blocks editing based on it **(unverified)**).
- scroll position and note mode (both local-only UI state).

Indices exist on account, category, favorite, shared, readonly, modified, remote id and status.

### 4.2 Account
Local id, server base URL, user name, SSO account name (identifier of the Files-app account), display name,
**collection ETag** and **collection Last-Modified** (used for incremental pulls), stored **API version list**
(string like a JSON array), theming color and text color, capabilities ETag, "direct editing available" flag.

### 4.3 Capabilities (single row, id fixed to 1)
Nextcloud major/minor/micro version, supported Notes API versions, theming color + text color, capabilities
ETag, direct-editing support flag, plus sharing-related flags (federated outgoing share allowed, public link
password enforced / asked, re-sharing allowed, default share permission) and whether user status supports "busy".
Note: despite being per-server data it is stored in one row, so with multiple accounts the latest fetched wins
for the sharing flags (observation, impact **unverified**).

### 4.4 CategoryOptions
Composite key (account id, category) with the chosen sorting method (modified desc / title asc / title desc).

### 4.5 Widget data
- Single-note widget data: widget id, account, note id, theme mode.
- Note-list widget data: widget id, account, mode (all / starred / by category), category, theme mode.

### 4.6 Share entity
Cached Nextcloud file shares for notes (share id, path, file target, share-with + display name, owners,
public URL, expiration, permissions, attributes). Only needed for the sharing feature.

### 4.7 Non-DB state
Shared preferences hold settings, current SSO account name, brand color and the three virtual-view sort orders.

---

## 5. Server API usage

All calls are relative to the server and routed through SSO; there is no direct HTTP client with
credentials in the app.

### 5.1 Notes API
Base paths: `/index.php/apps/notes/api/v1/` (API 1.x) and `/index.php/apps/notes/api/v0.2/` (legacy).

| Operation | Method and path | Notes |
|---|---|---|
| List notes (incremental) | `GET notes?pruneBefore=<epoch seconds>` + `If-None-Match: <ETag>` | See 6.2 |
| List ids only | `GET notes?exclude=etag,readonly,content,title,category,favorite,modified` | Used for the first import |
| Get one | `GET notes/{id}` | Used for first import, one request per note, up to 20 in parallel |
| Create | `POST notes` | Body: v1 sends the whole note object (title, content, category, favorite, modified, plus other serialized fields); v0.2 sends only category, modified, content, favorite |
| Update | `PUT notes/{id}` | Same bodies as create |
| Delete | `DELETE notes/{id}` | |
| Read settings | `GET settings` (v1 only) | Returns notes folder path and file suffix |
| Write settings | `PUT settings` (v1 only) | Sends only the field being changed |

Observations:
- No `If-Match` header is ever sent on update/delete, and no chunked pagination parameters are used.
  The server therefore cannot reject stale writes (see 6.4).
- Timestamps are serialized as epoch seconds both ways via a custom Gson adapter (also reused for OCS).
- v0.2 has no separate title: the client omits it and derives titles from content.

### 5.2 Capabilities / OCS
Base `/ocs/v2.php/cloud/`:
- `GET capabilities?format=json` with `If-None-Match` (ETag). Parsed selectively: Notes app section gives
  the supported `api_version` list; theming gives `color` and `color-text`; files section gives the direct
  editing flag (`directEditing.supportsFileId`); files_sharing and user_status flags as listed in 4.3;
  server `version` major/minor/micro.
- `GET users/{userId}?format=json` for the display name.

Other OCS bases (used by Android-only features): `/ocs/v2.php/apps/files/api/v1/` (direct editing),
`/ocs/v2.php/apps/files_sharing/api/v1/` (shares, sharees), `/ocs/v2.php/apps/user_status/api/v1/` (status).

---

## 6. Sync flow

### 6.1 Triggers
A full sync (push then pull) or push-only sync is scheduled:
- on app start / resume of the main screen and on account switch (full),
- by pull-to-refresh (capabilities first, then full sync),
- after every local change (push-only, immediately, i.e. each edit/save/delete/category change),
- when connectivity changes (full, for the current account) provided sync is currently allowed,
- by a periodic WorkManager job every 15 minutes if background sync is enabled (network required; syncs all
  accounts and waits for all to finish),
- a separate 24-hour periodic job refreshes capabilities for all accounts.

Connectivity gate: if "Wi-Fi only" is on, sync is considered impossible on non-Wi-Fi transport. When the
network is up but blocked by this rule the UI treats it as "offline by intention" (no error shown).

Concurrency rules: one sync task per account at a time. A push-only request is allowed even while a sync runs.
A full-sync request during a running sync is remembered and re-run afterwards. Callbacks (e.g. "save finished")
are attached to the next push or pull and fired on completion. A UI "syncing" flag drives the pull-to-refresh
spinner; an error list is exposed and shown as a snackbar with a "more" action.

### 6.2 Pull (server to local)
1. Re-read the account row to get the latest stored collection ETag and Last-Modified.
2. Build a map remote id -> local id for all non-deleted local notes of the account.
3. `GET notes` with `pruneBefore` set to the stored Last-Modified (epoch seconds) and `If-None-Match` set to
   the stored ETag. Per the public Notes API semantics **(inferred)**, notes unchanged since that timestamp are
   returned in pruned form (only id), and HTTP 304 means nothing at all changed. The client treats a returned
   note without a modified time as "unchanged".
4. For each returned note:
   - known remote id: update local row **only if** it has no pending local change (status void) **and** at
     least one of title, modified, favorite, category, ETag or content differs; update also regenerates the excerpt;
   - unknown remote id: insert as new, status void.
5. Every local note that has a remote id but is absent from the returned id list is deleted locally, but
   only if its status is void (so a locally edited note whose server copy vanished survives and gets
   re-created on the next push).
6. Persist the response headers: ETag and Last-Modified (if missing, 0) into the account row. Also parse the
   `X-Notes-API-Versions` response header and store it as the account's API version list; if it changed, the
   cached API client objects are discarded so the compatibility layer is rebuilt.
7. Special results: HTTP 304 counts as success; HTTP 503 (maintenance mode) counts as success; a network error
   counts as success (silently); an "API not responding" error from SSO resets the cached API client; any other
   error is collected and surfaced after the run.

### 6.3 Push (local to server)
Runs first. Loads all notes with non-void status for the account and processes them one by one:
- **local-edited, has remote id**: `PUT notes/{id}`. If the server answers 404 the note is re-created with
  `POST` (and its new remote id stored). Other failures are recorded as errors; the note stays pending.
- **local-edited, no remote id**: `POST notes`, store the returned remote id immediately.
- **local-deleted, no remote id**: just delete the local row (never reached the server).
- **local-deleted, has remote id**: `DELETE notes/{id}`; a 404 answer counts as success; then delete the local row.
- After a successful create/update the local row is overwritten with the server's response (modified, title,
  favorite, ETag, content, excerpt) and status reset to void, but **only if** content, favorite and category
  are still identical to what was pushed (optimistic check, so an edit made in the UI during the sync is not
  lost and stays pending for the next push). Note: category is part of that guard but is not copied back from
  the response during push.
- A 304 status from the SSO layer during push is ignored. Token mismatch errors reset the cached API client.
- One failing note does not stop the others; push reports overall success only if all succeeded.

### 6.4 Conflict handling (what actually happens)
There is no real conflict detection or merge UI:
- Pull never overwrites a locally pending note, so **local wins** until pushed.
- Push never checks the note's ETag against the server (no conditional write), so **last write wins** on the
  server; a concurrent remote edit made between the last pull and the push is silently overwritten.
- Per-note ETags are stored and used for the "changed?" comparison on pull, not for conditional requests.
- Deleted-remotely-but-edited-locally resolves to re-creation (404 fallback).
- Edited-remotely-and-locally: local content is pushed; the remote edit is lost **(inferred from the above)**.

### 6.5 Initial import (new account)
After the account is added: fetch the id-only list, then fetch each note individually (concurrently, up to
20 workers), inserting rows as void; per-note failures are collected as warnings and do not abort. Progress
(count/total) is reported to the UI. Fatal failures (cannot list ids) delete the freshly created account
and reset the current account. The first normal sync afterwards then stores ETag/Last-Modified (the import
itself does not).

### 6.6 Local editing semantics (offline behavior)
- Every save writes the row immediately with status local-edited and a fresh modified time, regenerates the
  excerpt, and requests a push-only sync. If the device is offline the note simply stays pending; nothing is
  lost across restarts because status is persisted.
- New notes are inserted locally with no remote id before any network call.
- Title handling: if the user explicitly sets a title it is sent as given. Otherwise, for accounts whose best
  common API is below 1.0, or for notes never synced, a placeholder title is replaced by one derived from the
  content; for synced notes on API 1.0 the existing title is retained (server renames file on its side
  **(inferred)**).
- Editor autosave: ~2 s after the last keystroke, then a ~5 s cool-down after the save/sync callback before
  another autosave may run. Also saves on pause and on instance-state save. An empty brand-new note is
  deleted again when the user leaves without typing or setting a title. "Cancel" in the menu restores the
  original state (deletes a new note, or re-saves the note as originally loaded).
- Deleting is a soft delete (status) followed by a push-only sync.
- **Favorite toggle is special**: the repository method performs an immediate `PUT` with the full note
  from the local DB with the favorite flag flipped, then schedules a full sync; it does not mark the row
  local-edited. A DAO method that would do an offline toggle exists but is not called from anywhere I
  found. So favoriting appears to require connectivity and may fail silently when offline **(verified in
  code paths, behavior on device not tested)**. The edit-screen menu also flips the in-memory flag first.
- Category change: sets category and local-edited, then push-only sync (works offline).

### 6.7 API version / capabilities negotiation
- Supported by the client: **1.0** and **0.2**. Constants for 1.2 and 1.3 exist only for gating two settings
  features (notes-path change needs >= 1.2, file-suffix change needs >= 1.3, though the version comparison
  helper for equality only looks at the major number).
- The server advertises its versions in the capabilities response (Notes section) and in the
  `X-Notes-API-Versions` header of notes list responses. The client keeps the list per account.
- Selection rule: from the server's list, keep versions the client supports (comparison by major version),
  choose the highest. If nothing matches or the list is unknown/empty, fall back to **0.2**.
- The selected version determines the Retrofit interface: v1 endpoints, or v0.2 endpoints with a reduced
  request body (no title). Settings endpoints exist only in the v1 interface; calling them under v0.2
  throws, so those UI actions assume v1.
- Title editing is only offered if the preferred version is at least 1.0.
- API client objects are cached per SSO account and invalidated when the version list changes, on token
  mismatch, on "API not responding", and on account removal.
- Capabilities refresh: on pull-to-refresh, on main-screen launch/resume path, and every 24 h; uses the stored
  ETag and treats 304 as "nothing to do". Result updates account branding color, API versions and
  direct-editing flag, and display name.

### 6.8 Categories, favorites on the wire
- Category is just the `category` string in each note. The client derives the category list locally by
  grouping notes (count per category); there is no category API call. Renaming or deleting a category is
  not offered as an operation (a category disappears when its last note leaves it).
- Favorite is the boolean `favorite` field.

---

## 7. Login flow (SSO, account import, credentials)

1. On start, if the account table is empty, the main screen opens the import screen.
2. The "add account" button calls the SSO library's account picker. It requires the **Nextcloud Files
   Android app** (it may be the production or other flavors of it; the app registers which app types to
   look for). If the Files app is not installed the user gets an error dialog explaining that. On devices
   where the picker needs the Android account-list permission, the permission is requested and the picker
   retried.
3. The picker returns an SSO account object (account name, server URL, user id). The library stores
   authentication material itself in the Files app / Android account manager; this app only stores the
   account name and marks it as current via the library helper.
4. Then, in a background thread: fetch capabilities (no ETag), fetch display name, insert capabilities and
   a new Account row, and run the initial import (6.5). On success the brand color is saved, background-sync
   work is (re)registered according to the preference, and the main screen is shown.
5. Failure handling: maintenance mode (503) and no network/DNS errors get friendly inline messages;
   anything else opens the exception dialog. All failure paths clear the "current account".
6. Adding a second account from the drawer uses the same picker; if the library reports a token mismatch for
   an account already present locally, the app treats it as "already imported" and just switches to it.
7. Selected account is the one the SSO library considers current; if the library no longer knows the stored
   account name (e.g. removed from the Files app) the main screen offers to back up unsynced notes via
   share sheet or show a debug report.
8. App code holds **no** username/password or token; no direct login-flow-v2 or app-password handling.

---

## 8. CORE functionality (portable concepts)

These do not depend on Android UI frameworks and are what a port should reproduce.

1. **Notes CRUD** against the Notes REST API v1 (and optionally 0.2), with local persistence and the
   status-based dirty tracking from 6.3 / 6.6.
2. **Categories**: free-text string per note, slash-separated hierarchy, counts per category,
   "uncategorized" virtual view, set category per note (also in bulk).
3. **Favorites**: boolean per note; "Favorites" virtual view.
4. **Search**: substring match on title and content within the current view and account (case-insensitive
   via SQL LIKE semantics), highlighting; separate in-note search with occurrence navigation.
5. **Sorting**: modified desc / title asc / title desc, stored per category, favorites first rule, section headers.
6. **Markdown**: edit view is plain text with live markdown styling (assumed from the library, **unverified**
   details); preview renders markdown including task-list checkboxes that can be toggled and written back;
   notes can also contain HTML-looking text (excerpt logic strips it; rendering of HTML in the markdown
   view **unverified**). Title/excerpt derivation strips markdown.
7. **Sync**: pull with ETag + Last-Modified + `pruneBefore`, push of dirty/deleted notes, 404 handling,
   optimistic local-vs-server reconciliation, maintenance-mode and offline tolerance, API version selection
   via capabilities and the `X-Notes-API-Versions` header, sync on connectivity regain and periodic sync.
8. **Login / accounts**: obtain server URL + credentials; multi-account data separation by account id.
   (The *mechanism* is Android-specific, see below; the *need* is core.)
9. **Offline**: full local copy, edits possible without network, queued push, no data loss on restart.

---

## 9. ANDROID-SPECIFIC functionality

| Area | What it does | Where |
|---|---|---|
| **SSO via Nextcloud Files app** | Credential-less login and transport; account picker; token handling | `importaccount/`, `persistence/ApiProvider`, `shared/util/SSOUtil` |
| **Home-screen widgets** | (a) Single-note widget showing one note's text, tap opens it; (b) note-list widget (all / starred / one category) with entries and a "new note" action. Config activities choose account + note/filter; widgets refresh after each sync/edit via a broadcast. | `widget/`, widget tables |
| **Share intents (inbound)** | Plain-text SEND creates a new note with shared text (also a Gmail/Assistant auto-send action); a second SEND handler for any text type opens the list so the user picks a note to append to (appended after a blank line); VIEW of `content://` text files opens a read-only viewer | `AndroidManifest.xml`, `AppendToNoteActivity`, `EditNoteActivity`, `ShareUtil` |
| **Share (outbound)** | System share sheet with note title and text; multi-note share concatenates contents with blank lines | `ShareUtil`, `MultiSelectedActionModeCallback` |
| **App lock** | Optional. Uses the device credential (PIN/pattern/password/biometric via keyguard confirm screen). Lock engages ~30 s after last interaction/leaving the root activity. Can only be enabled if the device has a secure lock. Separate option sets the secure-window flag to block screenshots and recents thumbnails | `LockedActivity`, `NotesApplication`, `DeviceCredentialUtil` |
| **Multi-account** | Several SSO accounts, drawer avatar, switch dialog, per-account data; lets notes be moved across accounts | `accountswitcher/`, `manageaccounts/`, `accountpicker/` |
| **Dark mode** | Three-way setting (light/dark/system) mapped to AppCompat night mode; some views switch backgrounds in dark mode | `preferences/DarkModeSetting`, `NotesApplication` |
| **Branding / server theming** | Reads the server theming color (and text color) from capabilities, stores it per account and globally, derives a Material color scheme and re-tints toolbar, FAB, dialogs, switches, progress, search bar, navigation; activity is recreated when the color changes | `branding/`, `Capabilities`, `CapabilitiesDeserializer` |
| **Quick-settings tile** | "New note" tile that unlocks the device if needed and opens the editor in a new task | `quicksettings/NewNoteTileService` |
| **Launcher shortcuts** | Static "create note" shortcut; dynamic shortcuts for the 4 most recently modified notes (rebuilt after sync, respecting rate limit); pin single note to the home screen; shortcut disabled when the note is deleted | `res/xml/shortcuts.xml`, `NotesRepository`, `BaseNoteFragment` |
| **Direct edit** | If the server advertises direct editing with file-id support and the user enabled it: obtain the note's file path from Notes settings, call the Files direct-editing "open" endpoint (`directEditing/open`, editor id "text", file id = note remote id), load the returned URL in a WebView with a JavaScript bridge for close/switch events and a load timeout; a FAB switches back to plain editing. A note without a remote id is first created on the server. | `edit/NoteDirectEditFragment`, `persistence/DirectEditingRepository`, `FilesAPI` |
| **Nextcloud sharing of notes** | Treats a note as a file at `/<notesPath>/<category>/<title><suffix>`, lists/creates/updates/deletes file shares (user, group, link, etc.) through `files_sharing` OCS API; link password/expiry dialogs; sharee search; public link copy; share cache table | `share/`, `ShareAPI` |
| **User status** | View and set online status and status message via `user_status` OCS API | `accountswitcher/` |
| **Ecosystem integration** | Drawer shortcuts to Nextcloud Files/Talk apps; handles an intent that hands over an account name | `MainActivity` |
| **Background work** | WorkManager periodic sync (15 min) and capabilities refresh (24 h) | `persistence/SyncWorker`, `CapabilitiesWorker` |
| **Connectivity observing** | Android connectivity callbacks drive "sync possible" flag and auto-sync | `shared/util/ConnectionLiveData`, `NotesRepository` |
| **Avatar loading** | Glide with SSO-authenticated fetch | `shared/util/CustomAppGlideModule` |
| **Crash reporter UI** | Custom uncaught-exception handler with tips and shareable report | `exception/` |
| **Touch UX** | Swipe actions, multi-select, drag/scroll-hide FABs, pull-to-refresh, soft keyboard handling, edge-to-edge | many |

---

## 10. Relevance for a PocketBook e-ink port

The PocketBook target is a native, non-Android e-ink device (SDK present in the workspace). It has no SSO
broker, no Android widgets/intents/keyguard, and has expensive screen refreshes and limited input.

**Port first (minimum viable client)**
1. Credentials and transport: since there is no Files app, authenticate directly (app password / Basic
   auth over HTTPS, or Login Flow v2 token) against the same REST endpoints. This replaces the whole SSO
   layer. Mind self-signed certificates and slow Wi-Fi.
2. Local store with the same record shape: remote id, status (void / edited / deleted), title, category,
   content, favorite, modified, ETag; plus per-account ETag + Last-Modified + API version string.
   A single file (SQLite or even flat files) is sufficient; one account is enough initially.
3. Sync engine exactly as in section 6: push dirty/deleted (PUT / POST / DELETE with the 404 fallbacks),
   then pull with `pruneBefore` + `If-None-Match`, delete only void notes missing remotely, treat
   304/503/offline as non-errors. Prefer API v1 only; read the supported versions from capabilities or the
   `X-Notes-API-Versions` header and refuse or degrade clearly if v1 is absent (v0.2 can be dropped).
   Consider adding what the Android app lacks: conditional writes with `If-Match` and a visible conflict
   outcome, since silent last-write-wins is risky.
4. Note list with categories (flat or one-level drawer/menu equivalent), favorites flag, sort by modified or
   title, simple substring search on title/content.
5. Reader view with basic markdown (headings, emphasis, lists, task-list checkboxes, links) rendered for
   e-ink (static, no animation); toggling checkboxes is a cheap, valuable interaction.
6. Editor: plain-text editing is enough; save explicitly or on leaving (an autosave timer is less useful on
   e-ink with an on-screen or external keyboard); create note, delete note, set category, toggle favorite.
7. Manual sync trigger plus sync on start/when Wi-Fi is available.

**Port later / optional**
- Multiple accounts; move note between accounts.
- In-note search; internal note links (numeric remote id links).
- Excerpt in the list (cheap to generate; good for reading lists on a large e-ink screen).
- Periodic background sync (only if the platform allows background tasks and battery use is acceptable).
- Notes settings (notes folder path/suffix) - probably unneeded.
- Capabilities-driven theming is not needed; maybe only show server version.

**Drop**
- SSO/Files-app dependency, account import via Android account manager, token-mismatch handling.
- Widgets, quick-settings tile, launcher shortcuts and pinning, share-intent receivers, read-only VIEW intent.
- App lock via keyguard, screenshot blocking.
- Branding/server color theming, dark mode (e-ink is monochrome; offer high-contrast and font size instead),
  animations, swipe gestures, grid view, FAB hide-on-scroll, pull-to-refresh (use a button).
- Direct edit WebView, Nextcloud file sharing UI, user-status editing, trash bin link, ecosystem banner,
  avatar loading, crash-report UI.

**E-ink specific adaptations to consider** (not in the Android app): large touch targets and paged (not
scrolled) reading, minimal redraws, font size control (the Android app has three sizes, a monospace option
and a keep-screen-on option, which map naturally), and an explicit "sync now" with a clear last-sync status
since there is no pull-to-refresh spinner.

---

## 11. Open points not verified

- Exact markdown feature set and live-styling behavior of the editor/renderer (delegated to an external
  library whose source was not analyzed).
- Whether the read-only flag from the server is enforced anywhere in the UI.
- Whether "Uncategorized" is hidden when empty (drawer assembly code only partly read).
- Behavior of the server for `pruneBefore` and title/file renaming is described from the public API
  semantics, not from server code.
- Real-device behavior of offline favoriting (code paths show it needs the network).
- Capabilities table being a single shared row for multiple accounts and its consequences.
- Trash-bin and ecosystem integrations were only skimmed.
