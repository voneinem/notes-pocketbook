# Notes Pocketbook

A notes app for the PocketBook InkPad One that syncs with a [Nextcloud Notes](https://github.com/nextcloud/notes)
server. Read your notes on e-ink, also offline; write and edit with the on-screen keyboard; sync
when you are on Wi-Fi.

> **Status: specification stage.** The specification, plan and task list exist under
> [`specs/001-pocketbook-notes-mvp/`](specs/001-pocketbook-notes-mvp/). No application code has been
> written yet, so the build and install instructions below describe the planned workflow and have
> not been verified. Working name of the app and binary: `pbnotes` (a placeholder).

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
  skip verification, and none to trust a self-signed certificate in the MVP. If your server uses a
  self-signed certificate, make it trusted on the device first.
- **The password is stored unencrypted.** The Nextcloud password is saved in clear text in
  `pbnotes.db` under `/mnt/ext1/system/pbnotes/` on the device's FAT partition, which anyone with
  access to the device or its USB storage can read. Use a Nextcloud **app password** (revocable,
  limited), never your main account password. "Forget account" overwrites the stored password.
- Note contents and credentials are never written to logs.

## Building (planned)

Requirements on the host (Linux x86-64): `cmake` 3.16+, `ninja` or `make`, a C++17 compiler,
the libcurl development package, `zip`, and for the device build the Qt host tool `rcc` from
`qt6-base-dev-tools` (the SDK's own `rcc` does not run on current hosts).

Host build and tests (no Qt, SDK or device needed):

```bash
cmake -S . -B build-host -G Ninja
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Device build (needs the PocketBook SDK; its location comes only from the environment, there is no
default path):

```bash
export POCKETBOOK_SDK=/path/to/arm-buildroot-linux-gnueabihf_sdk-buildroot   # or pass -DSDK_PATH=...
cmake -S . -B build-arm -G Ninja -DTARGET_TYPE=ARM -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm
scripts/package-device.sh build-arm/pbnotes.app "" build-arm/dist
```

More detail and the end-to-end checks: [quickstart.md](specs/001-pocketbook-notes-mvp/quickstart.md).

## Installing on the device (planned)

1. Unzip the package onto the reader's storage root (`/mnt/ext1`), merging `applications/`.
2. Back up `system/config/desktop/view.json`, add an entry with an id starting with `U_`, an absolute
   `path` to `applications/pbnotes.app`, and the icon paths, then add it to an application group.
3. Reboot and start the app from Applications.

The package's `INSTALL.md` gives the exact entry.

## Documentation

- [Constitution](.specify/memory/constitution.md): project rules (test-first, simplicity, security,
  host-testable core, no silent data loss)
- [Specification](specs/001-pocketbook-notes-mvp/spec.md), [plan](specs/001-pocketbook-notes-mvp/plan.md),
  [tasks](specs/001-pocketbook-notes-mvp/tasks.md)
- Background analyses in [`docs/`](docs/): InkFeed, the PocketBook SDK, and the behavior of the
  Android Notes app (reference only)

## License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE).
Copyright (c) 2026 Volker von Einem.

Third-party components and adapted patterns (InkFeed, MIT; the Mozilla CA bundle, MPL-2.0; SQLite,
public domain) are attributed in `NOTICE`. The Android Notes app (GPL-3.0) is used as a behavioral
reference only; no code or text from it is included.
