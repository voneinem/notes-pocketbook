# Data Model: PB Store Release Package

No database or runtime data changes. These are the files and values the feature handles.

## Manifest template (`packaging/manifest.template.json`, in the repository)

| Field | Value | Source |
|-------|-------|--------|
| `schema_version` | `"1.0"` | fixed |
| `id` | `"pbnotes"` | fixed |
| `name` | `"PB Notes"` | fixed |
| `version` | `"@VERSION@"` | placeholder, replaced at build time |
| `min_firmware` | `"6.11.1437"` | fixed (author's InkPad One) |
| `target_device` | `"InkPad One"` | fixed |
| `menu_entry.id` | `"U_PbNotes"` | fixed |
| `menu_entry.title` | `"PB Notes"` | fixed |
| `menu_entry.path` | `"applications/pbnotes.app"` | fixed |
| `menu_entry.icon` | `"applications/icons/pbnotes.bmp"` | fixed |
| `menu_entry.focused_icon` | `"applications/icons/pbnotes_f.bmp"` | fixed |
| `menu_entry.group` | `"@General"` | fixed |
| `data_dir` | `"system/pbnotes"` | fixed |

`files` is **absent** from the template; `pbstore-catalog pack` adds it (path, size, SHA-256, sorted).

Rules (store contract, schema 1.0): `menu_entry.path/icon/focused_icon` must be listed in `files`; `data_dir` is not inside
`applications/`; the package never contains files in it.

## Package manifest (`manifest.json` inside the ZIP)

The template with `@VERSION@` replaced and `files` added. Invariants:
- `version` = content of `VERSION` = version in the ZIP name = the version compiled into `pbnotes.bin`.
- every file under `applications/` is in `files`, every `files` entry exists, sizes and SHA-256 match (written by `pack`).

## Release package (`dist/pbnotes-inkpad-one-<version>.zip`)

```text
manifest.json
applications/pbnotes.bin, pbnotes.app, pbnotes-cacert.pem, pbnotes-emoji.ttf, pbnotes-emoji-OFL.txt
applications/pbnotes-licenses/…        (manifest.txt and every text it names, as in 1.10.0)
applications/icons/pbnotes.bmp, pbnotes_f.bmp
```

Top level: `manifest.json` and `applications/` only. No `INSTALL.md`. The `applications/` file list equals the 1.10.0 one
(the old ZIP also had directory entries; the packed ZIP has none, the files are the same).

State: *staged* (temp dir) → *packed twice* → *identical* → *checked* → *published* (moved into `dist/`). A failure at any
step ends the build with no new ZIP in `dist/`.

## Release

| Part | Rule |
|------|------|
| Git tag | `v<version>`, created after the merge, equals `v` + manifest version |
| GitHub release | for that tag; the ZIP attached; notes = changelog entry + link to `INSTALL.md` at that tag |
| Verified by | `scripts/check-release.sh` (tag, `VERSION`, ZIP name, manifest version) |
