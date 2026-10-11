# Notes Pocketbook

A notes app for the PocketBook InkPad One that syncs with a [Nextcloud Notes](https://github.com/nextcloud/notes)
server. Read your notes on e-ink, also offline; write and edit with the on-screen keyboard; sync
when you are on Wi-Fi.

> **Status: MVP complete and tested on an InkPad One; rich note content (links, tables, emojis,
> attachments) is implemented and tested on the host, its device checks are still open.** Early
> software; it works for the author.
>
> - **Works on the device (MVP):** setup with a connection test, note list with views, sorting and
>   search, reader (basic Markdown, task checkboxes, font size), editor with explicit save, offline
>   create/edit/delete, manual and start-time sync, and conflict handling that keeps both
>   versions. The device results are recorded in [research.md](specs/001-pocketbook-notes-mvp/research.md)
>   (R-15 to R-20).
> - **Rich note content (feature 002):** web links open in the reader's browser, Markdown tables are
>   drawn as grids, emojis are kept and shown, and attachments can be viewed and added. Implemented
>   and covered by host tests; the on-device checks (browser return, emoji font, image formats, viewer
>   hand-off, upload against a real server) are listed as manual tasks in
>   [tasks.md](specs/002-rich-note-content/tasks.md) and not yet recorded.
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
  [tasks](specs/002-rich-note-content/tasks.md)
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
