# Contract: `licenses/manifest.txt`

The single declaration of the app and its third-party components. Hand-edited, read by the app at
run time, checked by the consistency test. A change to this format is a contract change and needs
a matching change to `test_about_files`.

## Location

- Repository: `licenses/manifest.txt`.
- Device: `/mnt/ext1/applications/pbnotes-licenses/licenses/manifest.txt` — the directory
  `pbnotes-licenses/` mirrors the repository, so every path in the manifest is valid unchanged on
  the device and on the host.
- **Licenses root** = repository root on the host, `pbnotes-licenses/` on the device
  (`PBNOTES_LICENSES_ROOT`, overridable by an environment variable of the same name on the host).
  The manifest is always `<root>/licenses/manifest.txt`.

## Grammar

```text
file     = { block | blank | comment }
block    = line { line }            ; lines until the next blank line
line     = key ":" SP value LF
comment  = "#" any LF               ; whole line only
key      = [a-z][a-z0-9_]*
value    = text without LF, trimmed
```

- UTF-8, LF or CRLF line ends, no BOM required (a BOM is skipped).
- List values are comma-separated; items are trimmed; empty items are ignored.
- A block with a line that is not `key: value` is a **bad block**.
- Unknown keys are ignored (forward compatibility).
- A repeated key within a block: the last one wins.

## Keys

| Key | Applies to | Required | Notes |
|-----|-----------|----------|-------|
| `id` | all | yes | `[a-z0-9-]+`, ≤ 40 characters, unique |
| `kind` | all | yes | `app` (exactly one block) or `component` |
| `name` | all | yes | ≤ 80 characters |
| `purpose` | component | yes | ≤ 120 characters |
| `license` | all | yes | license name as shown, ≤ 120 characters |
| `holder` | all | yes | copyright holder(s); for the app this is the author's name |
| `copyright` | app | yes | the copyright line shown on the About screen |
| `url` | component | yes | `https://…`, ≤ 200 characters |
| `origin` | component | yes | `bundled` or `system` |
| `text` | all | no | list of paths of license texts below the licenses root |
| `source` | component | no | list of repository paths the entry stands for (consistency test only) |
| `cmake` | component | no | list of CMake package/library names the entry stands for (consistency test only) |

## Limits and path rules (external input)

| Rule | Value |
|------|-------|
| Manifest size | ≤ 64 KiB; larger → the whole manifest is unavailable (the screens show the unavailable marker) |
| Blocks read | ≤ 64; further blocks are ignored |
| Text file size | ≤ 256 KiB; larger files are cut at the cap and marked "Text shortened." |
| `text` path | relative; no leading `/`; no `..` segment; no `\`; no NUL; ≤ 200 characters; resolved only below the licenses root |
| Symlinks | not followed outside the licenses root (the resolved real path must stay below it) |

A bad block (syntax, missing required key, over-long value, duplicate `id`, a second `kind: app`,
invalid path) is dropped from the list and counted; the list page shows one extra line
"N entries could not be read." when the count is not zero. A missing manifest or a manifest with no
`app` block makes the app fields "unavailable" and the list empty with the same message
(FR-016).

## Example

```text
id: sqlite
kind: component
name: SQLite
purpose: Stores the notes on the reader
license: Public domain
holder: D. Richard Hipp and the SQLite contributors
url: https://www.sqlite.org/copyright.html
origin: bundled
source: third_party/sqlite

id: qt
kind: component
name: Qt 6.8 (Core, Gui, Qml, Quick)
purpose: Draws the screens and runs the app
license: GNU Lesser General Public License 3 (LGPL-3.0)
holder: The Qt Company Ltd. and other contributors
url: https://www.qt.io/licensing/
origin: system
text: licenses/LGPL-3.0.txt, licenses/GPL-3.0.txt
cmake: Qt6
```

## Consistency rules (checked by `test_license_consistency`, run under CTest)

1. Every `component` block has a `NOTICE` section whose heading line starts with the block's
   `name`, and every `NOTICE` section (text after a line of 80 dashes) has a block.
2. Every directory directly under `third_party/` appears in some block's `source`.
3. Every package named in a `find_package(<Name>` call, and every linked library named in
   `target_link_libraries` (other than the project's own targets, `pthread`, `m` and test-only
   packages named in `tests/CMakeLists.txt`), appears in some block's `cmake`.
4. Every `text` and `source` path exists; every `text` file is non-empty UTF-8 and ≤ 256 KiB.
5. The app block's `copyright` equals the first line of `LICENSE` that starts with `Copyright`.
6. The manifest parses with no bad blocks.

Each rule has a negative test on in-memory data proving it reports a finding; the test names the
rule number and the offending entry in its message.

## Packaging contract

`scripts/package-device.sh` installs `licenses/manifest.txt` and every file named by any `text`
key into `pbnotes-licenses/` keeping the relative path (`install -D`), and fails if a named file
is missing. Nothing else from `licenses/` is installed (not `SOURCES.md`).
