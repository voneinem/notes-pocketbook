# Notes Pocketbook

A notes app for the PocketBook InkPad One that syncs with a [Nextcloud Notes](https://github.com/nextcloud/notes)
server. Read your notes on e-ink, also offline; write and edit with the on-screen keyboard; sync
when you are on Wi-Fi.

> **Status: MVP complete and tested on an InkPad One; rich note content (links, tables, emojis,
> attachments), the About box and the explicit title with a category picker are implemented and tested on the host,
> their device checks are still open.** Early
> software; it works for the author.
>
> - **Works on the device (MVP):** setup with a connection test, note list with views, sorting and
>   search, reader (Markdown, task checkboxes, font size), editor with explicit save, offline
>   create/edit/delete, manual and start-time sync, and conflict handling that keeps both
>   versions. The device results are recorded in [research.md](specs/001-pocketbook-notes-mvp/research.md)
>   (R-15 to R-20).
> - **Rich note content (feature 002):** web links open in the reader's browser, Markdown tables are
>   drawn as grids, emojis are kept and shown, and attachments can be viewed and added. Implemented
>   and covered by host tests; the on-device checks (browser return, emoji font, image formats, viewer
>   hand-off, upload against a real server) are listed as manual tasks in
>   [tasks.md](specs/002-rich-note-content/tasks.md) and not yet recorded.
> - **Editing tools and the reading page (feature 005):** a long-press menu with draggable selection markers (Cut,
>   Copy, Paste, Remove, Select all, Undo, Redo) in the editor; the reading page shows the note's title and can select
>   and copy text the same way. Implemented
>   and covered by host tests (including the real QML pages driven offscreen); the device checks are open, see
>   [device-results.md](specs/005-text-editing-reader-title/checklists/device-results.md).
> - **Better Markdown and the "/" menu (feature 006):** code blocks, clearer headings, quotes, info boxes, strikethrough,
>   underline and collapsible details on the reading page; a "/" menu, a "//" date-time picker and an emoji grid in the
>   editor. Implemented and covered by host tests (including the real QML pages driven offscreen); the device checks
>   are open, see [quickstart](specs/006-markdown-slash-menu/quickstart.md).
> - **Tested on the host:** the Qt-free core, the bridge and the QML pages are covered by automated
>   tests. Run `scripts/pre-merge.sh` to build, run them and check for secrets.
> - **Not verified:** the Notes API details that need a live server (the fixtures in
>   `tests/fixtures/` are hand-written and marked `.unverified`), placing your own
>   `custom-ca.pem` over USB, and long typing sessions (latency, ghosting). Only the InkPad One was
>   tested. See [tasks.md](specs/001-pocketbook-notes-mvp/tasks.md) and
>   [docs/security-review.md](docs/security-review.md).
>
> Working name of the app and binary: `pbnotes` (a placeholder).

> **Built on [InkFeed](https://github.com/jrohde/InkFeed) by Joost Rohde.**
> InkFeed is the open-source RSS/Atom reader for the PocketBook InkPad One that showed how to run a
> Qt Quick app on this device with the PocketBook SDK. This project would not exist without it. See
> [Credits](#credits-and-thanks).

Not affiliated with Nextcloud GmbH or PocketBook.

## What it does

### MVP

Setup with server URL, username and app password; note list with views (All, Favorites,
Uncategorized, by category), sorting and search; reader with basic Markdown and toggleable
task-list checkboxes; plain-text editor with explicit save; create, delete, category, favorite,
all working offline; manual and start-time sync; conflicts keep both versions. Out of scope:
widgets, multiple accounts, app lock, sharing, dark mode, direct edit, internal note
links, background sync. Details: [spec.md](specs/001-pocketbook-notes-mvp/spec.md).

### Rich note content

Details: [spec.md](specs/002-rich-note-content/spec.md), [quickstart](specs/002-rich-note-content/quickstart.md).

- **Links.** `http://` and `https://` links (titled, bare, `<autolinks>`) show underlined and open in the
  reader's browser on one tap. If the link text is not itself the address, a prompt first shows the real
  address. Other schemes (`mailto:`, `javascript:`, ...) stay plain text.
- **Tables.** GitHub-style pipe tables are drawn as a grid that fits the screen: bold header, left/center/
  right alignment, wrapping cells, links and emphasis inside cells. A page break never cuts a row. The
  editor still shows the original pipe text.
- **Emojis.** Emojis are kept byte for byte through editing and sync, found by search, and never cut in
  half. They are drawn with a bundled monochrome font (Noto Emoji, SIL OFL 1.1, installed as
  `applications/pbnotes-emoji.ttf` with its license `applications/pbnotes-emoji-OFL.txt`); if the font is
  missing the device font is used.
- **Attachments.** Files that notes reference appear in place with name and, when known, size.
  **Nothing is downloaded by a sync**: tap *Download* to fetch a file (limit **10 MiB** per file). Pictures
  show in place (grayscale) and full screen on a tap; other files open in the reader's viewer when it
  supports the type. Downloaded files work offline. In the editor, *Attach* picks a file from the reader's
  storage; it is copied into the app and uploaded at the next sync, before the note text refers to it.
  Needs a Nextcloud Notes server with API version 1.4. Interrupted uploads lose nothing; a conflict copy
  keeps its attachments.
- **Upgrade is one way.** The first start of this version upgrades the local database to schema 2 (a
  backup is made and removed after the check). An older build of the app refuses an upgraded database.

### Versions

The release version is the file [VERSION](VERSION); the app, the About screen and the package name all use it.
Every feature raises it and adds an entry to [CHANGELOG.md](CHANGELOG.md); `scripts/pre-merge.sh` fails
otherwise (`scripts/check-version-bump.sh`). After the merge, tag the release: `git tag v$(cat VERSION)`.

### About box

Settings → *About this app* shows the app's name, version, author, copyright and license. *License* shows
the full license text; *Third-party licenses* lists every library, code base, font and data file the app
uses (including Qt, libcurl and OpenSSL, which the reader provides) with its license and full text, all
offline. Details: [spec.md](specs/003-about-box/spec.md), [quickstart](specs/003-about-box/quickstart.md).

The list comes from one manifest, [licenses/manifest.txt](licenses/manifest.txt); the install package carries
it and the license texts it names as `applications/pbnotes-licenses/`. **To add a dependency:** add a block
to the manifest and a section to [NOTICE](NOTICE) (its heading starts with the component's name); the test
`test_license_consistency` fails the build when a vendored directory, a CMake dependency, a `NOTICE` section
or a license file does not match the manifest.

### Explicit title and category picker

The editor has a separate **Title** field. The title is what you type there: it is never taken from the
first line of the text, editing the text never changes it, and it is sent to the server with the note (the
server may adjust it, for example to make it unique, and the reader then shows the server's title). A note
cannot be saved without a title. The **Category** control opens a list with *No category*, *New category…*
and every category already in use; choosing one needs no typing. *New category…* asks for a name once and
offers it to all notes as soon as a note carrying it is saved. Details: [spec.md](specs/004-title-category-picker/spec.md),
[quickstart](specs/004-title-category-picker/quickstart.md).

- **Upgrade is one way (again).** The first start of this version upgrades the local database to schema 3
  (a backup is made and removed after the check): notes you edited but had not synced keep the title the
  list showed, and the list previews are recomputed. No note is renamed or recategorized by the upgrade.
  An older build of the app refuses an upgraded database.

### Editing tools and the title in the reader

The editor header only has **Cancel, Save, Favorite, Attach** (and **Delete** for a saved note). The editing tools are
in a context menu, like the reader's own: **long-press a word** to select it; two black **markers** appear at the ends
of the selection and can be dragged, and a **bubble** offers *Cut, Copy, Paste, Remove, Select all, Undo, Redo* (only
those that apply). A long press on empty space below the text offers *Paste, Select all, Undo, Redo*; a short tap just
places the caret. The menu works on the field you pressed, the title or the text, and never closes the on-screen
keyboard. Pasting into the title makes one line (255 characters at most). **Undo/Redo** step back and forth over every
change to the title, the text, the category and the favorite marker; undoing everything clears the "unsaved changes"
prompt, and the history is not saved. The clipboard lives in the app (and uses the system clipboard where the reader's
platform offers one); text pasted from outside is cleaned first.

The reading page shows the note's **title** above the text, in bold and larger, and can **select and copy** the same
way: long-press a word, drag the markers (also across blocks), choose **Copy** in the bubble to put the note's own
source text on the clipboard (a completely selected heading, bold phrase or link brings its marks, a partly selected one
does not). The page stays read-only. Details: [spec.md](specs/005-text-editing-reader-title/spec.md),
[quickstart](specs/005-text-editing-reader-title/quickstart.md).

### Better Markdown and the "/" menu

Details: [spec.md](specs/006-markdown-slash-menu/spec.md), [quickstart](specs/006-markdown-slash-menu/quickstart.md).

- **Reading.** Fenced **code blocks** (three or more backticks or tildes, optional language such as `yaml`) are shown as
  one boxed area in fixed-width type with every line and space as written; nothing inside is formatted and no link in it
  can be followed. `#` to `######` headings get distinct sizes, and a heading is never the last line of a page.
  `**bold**`, `~~strikethrough~~` and `<u>underline</u>`; **quotes** (`> `) with a bar at the left; **info boxes**
  (`> [!NOTE]`, also `[!TIP]`, `[!WARNING]`, `[!IMPORTANT]`, `[!CAUTION]`) in a box that names its kind in words; and
  collapsible **`<details>`** sections that show only their `<summary>` line until you tap it.
- **Writing.** In the editor, typing **`/`** at the start of a line or after a space opens a list of twelve formats next to
  the caret: bullet list, table, info box, link, bold, italic, underline, strikethrough, details, quote, code block, emoji.
  Choosing one inserts its Markdown with the caret where you continue typing; `a/b`, `1/2` and `https://` are not
  affected, and the Title field has no menu. **`//`** opens a date-time picker (it starts at now; Insert writes
  `YYYY-MM-DD HH:MM`), and *Emoji* opens a grid of 48 common emojis. Each insertion is one Undo step. Notes stay plain
  Markdown: other apps show the same text, in their own way.

## Security notes

- **Certificates are always verified.** Only `https://` servers are accepted. There is no option to
  skip verification, and no in-app option to accept an unverifiable certificate. If your server
  uses a private CA or a self-signed certificate, place that certificate file as `custom-ca.pem`
  in the app's data folder (`/mnt/ext1/system/pbnotes/`); the Settings screen will show whether a
  custom CA is loaded. Verification stays on.
- **The password is stored unencrypted.** The Nextcloud password is saved in clear text in
  `pbnotes.db` under `/mnt/ext1/system/pbnotes/` on the device's FAT partition, which anyone with
  access to the device or its USB storage can read. Use a Nextcloud **app password** (revocable,
  limited), never your main account password. "Forget account" overwrites the stored password and
  leaves no trace of it in the database files (tested).
- Note contents and credentials are never written to logs.
- The review of what has been verified, and what is still open, is in
  [docs/security-review.md](docs/security-review.md).

## Building and testing

Requirements on the development machine (Linux x86-64):

- `cmake` 3.16 or later, `ninja` (or `make`), a C++17 compiler
- libcurl development package, version 7.85 or later (Debian/Ubuntu: `libcurl4-openssl-dev`)
- OpenSSL development package and the `openssl` command (the TLS tests start local HTTPS servers)
- `zip` (packaging)
- For the device build and the optional desktop app: the Qt6 base and declarative development
  packages, and the host Qt tools (`qt6-base-dev-tools`, which provides `rcc`; the SDK's own `rcc`
  does not run on current hosts)

Host build and tests (no Qt, SDK or device needed for the core and its tests):

```bash
cmake -S . -B build-host -G Ninja
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

The merge gate runs the build, all tests and a secret scan in one go:

```bash
scripts/pre-merge.sh
```

Device build (needs the PocketBook SDK; its location comes only from the environment, there is no
default path):

```bash
export POCKETBOOK_SDK=/path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot   # or pass -DSDK_PATH=...
cmake -S . -B build-arm -G Ninja -DTARGET_TYPE=ARM -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm
file build-arm/app/pbnotes.app          # ELF 32-bit LSB executable, ARM, EABI5
scripts/package-device.sh build-arm/app/pbnotes.app "" build-arm/dist
```

More detail and the end-to-end checks: [quickstart.md](specs/001-pocketbook-notes-mvp/quickstart.md).

## Installing on the device

See [INSTALL.md](INSTALL.md) (also part of the package). In short: copy `applications/` to the
reader's storage root, register the app in `system/config/desktop/view.json`, reboot. These steps
were used on an InkPad One.

## Documentation

- [Constitution](.specify/memory/constitution.md): project rules (test-first, simplicity, security,
  host-testable core, no silent data loss)
- [Specification](specs/001-pocketbook-notes-mvp/spec.md), [plan](specs/001-pocketbook-notes-mvp/plan.md),
  [tasks](specs/001-pocketbook-notes-mvp/tasks.md); rich note content:
  [specification](specs/002-rich-note-content/spec.md), [plan](specs/002-rich-note-content/plan.md),
  [tasks](specs/002-rich-note-content/tasks.md); About box:
  [specification](specs/003-about-box/spec.md), [plan](specs/003-about-box/plan.md),
  [tasks](specs/003-about-box/tasks.md)
- Background analyses in [`docs/`](docs/): [InkFeed](https://github.com/jrohde/InkFeed), the PocketBook SDK, and the behavior of the
  Android Notes app (reference only)

## Credits and thanks

**Many thanks to Joost Rohde ([@jrohde](https://github.com/jrohde)) for
[InkFeed](https://github.com/jrohde/InkFeed).** Without that project this app would have started
from zero: InkFeed proved that a Qt 6 / Qt Quick application can run on the InkPad One, and it
documents how to get there. This project follows its approach and adapts these parts of it:

- the CMake ARM cross-build setup with the PocketBook SDK, and the manual `moc`/`rcc` steps
  (`cmake/sdk.cmake`, `cmake/qt_manual.cmake`)
- the InkView glue, rotation and `devicePixelRatio` handling, and the flat e-ink button style
  (`app/`, `qml/`)
- the launcher script that starts the app on the reader (`platform/pocketbook/pbnotes.app`)
- the packaging script that builds the install zip (`scripts/package-device.sh`)

InkFeed is MIT licensed, Copyright (c) 2026 Joost Rohde; its license text is reproduced in
[NOTICE](NOTICE). If you like this app, please have a look at InkFeed and give it a star.

## License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE).
Copyright (c) 2026 Volker von Einem.

Third-party components and adapted patterns ([InkFeed](https://github.com/jrohde/InkFeed) by Joost Rohde, MIT; the Mozilla CA bundle, MPL-2.0; SQLite,
public domain; Noto Emoji, SIL OFL 1.1) are attributed in [NOTICE](NOTICE). The Android Notes app (GPL-3.0) is used as a
behavioral reference only; no code or text from it is included.
