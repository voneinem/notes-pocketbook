# Notes Pocketbook

A notes app for the PocketBook InkPad One that syncs with a [Nextcloud Notes](https://github.com/nextcloud/notes)
server. Read your notes on e-ink, also offline; write and edit with the on-screen keyboard; sync
when you are on Wi-Fi.

> **Status: work in progress.**
>
> - **Done and tested on the host:** the Qt-free core library: Notes API v1 client, SQLite store,
>   push-then-pull sync with conflict handling (both versions are kept), markdown parser, search,
>   views and sorting, TLS with certificate verification and an optional user-supplied CA file.
>   Run `scripts/pre-merge.sh` to see the 41 automated tests and the checks pass.
> - **Built for the device but not yet run on one:** a Hello World app with a typing spike page,
>   the launcher script and the install package (`pbnotes.bin`, `pbnotes.app`).
> - **Not done yet:** the real user interface (setup, note list, reader, editor, settings) and
>   everything that needs the device. See [tasks.md](specs/001-pocketbook-notes-mvp/tasks.md):
>   tasks marked `[MANUAL]` need the InkPad or a live Nextcloud server.
>
> Working name of the app and binary: `pbnotes` (a placeholder).

> **Built on [InkFeed](https://github.com/jrohde/InkFeed) by Joost Rohde.**
> InkFeed is the open-source RSS/Atom reader for the PocketBook InkPad One that showed how to run a
> Qt Quick app on this device with the PocketBook SDK. This project would not exist without it. See
> [Credits](#credits-and-thanks).

Not affiliated with Nextcloud GmbH or PocketBook.

## MVP scope

Setup with server URL, username and app password; note list with views (All, Favorites,
Uncategorized, by category), sorting and search; reader with basic Markdown and toggleable
task-list checkboxes; plain-text editor with explicit save; create, delete, category, favorite,
all working offline; manual and start-time sync; conflicts keep both versions. Out of scope:
widgets, multiple accounts, app lock, sharing, dark mode, attachments, direct edit, internal note
links, background sync. Details: [spec.md](specs/001-pocketbook-notes-mvp/spec.md).

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
have not been verified on a device yet.

## Documentation

- [Constitution](.specify/memory/constitution.md): project rules (test-first, simplicity, security,
  host-testable core, no silent data loss)
- [Specification](specs/001-pocketbook-notes-mvp/spec.md), [plan](specs/001-pocketbook-notes-mvp/plan.md),
  [tasks](specs/001-pocketbook-notes-mvp/tasks.md)
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
public domain) are attributed in [NOTICE](NOTICE). The Android Notes app (GPL-3.0) is used as a
behavioral reference only; no code or text from it is included.
