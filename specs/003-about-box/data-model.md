# Data Model: About Box

Nothing is stored or synced. All entities are read from files at run time and held in memory while
a screen is open. Formats: [contracts/manifest-format.md](contracts/manifest-format.md).

## AppInfo (one per installed build)

| Field | Source | Rule |
|-------|--------|------|
| `name` | manifest, block `kind: app` | required; shown as the screen title and name line |
| `version` | `pbnotes::version()` (set at build time) | shown via `display_version()`: empty or `0.0.0` → "unknown" |
| `author` | manifest `holder` of the app block | required; "unavailable" marker if the block is missing |
| `copyright` | manifest `copyright` of the app block | shown as is; must equal the first `Copyright` line of `LICENSE` (consistency test) |
| `license` | manifest `license` of the app block | license *name*, e.g. "MIT License" |
| `text` | manifest `text` of the app block | path(s) of the license text below the licenses root |

If the manifest cannot be read, `name` and `version` still come from the build (name is a
constant of the app), and `author`, `copyright` and `license` show the unavailable marker (FR-016).

## Component (a third-party library, code base, font or data file)

| Field | Manifest key | Rule |
|-------|--------------|------|
| `id` | `id` | required, `[a-z0-9-]+`, unique in the manifest |
| `name` | `name` | required, one line, ≤ 80 characters |
| `purpose` | `purpose` | required, one line, ≤ 120 characters ("what the app uses it for") |
| `license` | `license` | required, license *name* as a person would say it (not an SPDX list) |
| `holder` | `holder` | required, copyright holder(s) as stated by the component; for components without a stated holder, the maintaining organisation |
| `url` | `url` | required, `https://` address where the component or its license is published; shown as text, never opened |
| `origin` | `origin` | `bundled` (ships in the package or compiled in) or `system` (provided by the reader) |
| `text` | `text` | optional list of paths below the licenses root; empty = no text exists or is distributed |
| `source` | `source` | optional list of repository paths this entry stands for (e.g. `third_party/sqlite`); used only by the consistency test |
| `cmake` | `cmake` | optional list of CMake package/library names this entry stands for; used only by the consistency test |

`available` (derived): false when the block failed validation (the entry is then shown with
`name` = its `id` or "Unavailable entry" and a reason) or when `text` is non-empty and none of its
files could be read. A component with an empty `text` is *available*: its page shows the fields
above, with the line "The full license text is published at <url>."

## LicenseText (one per text file)

| Field | Rule |
|-------|------|
| `paragraphs` | list of strings: the file split at blank lines; line breaks inside a paragraph kept; leading/trailing blank lines dropped; a file with more than 5,000 paragraphs is truncated at that count with a final "Text shortened." paragraph |
| `truncated` | true if the size cap (256 KiB) cut the file; a final "Text shortened." paragraph is appended |

A component with several text files shows them one after the other, each preceded by a one-line
heading that is the file's base name without extension.

## Manifest (the list)

Ordered list of blocks in file order: first the app block, then components. The list page shows
`bundled` entries first, then `system` entries, keeping manifest order inside each group.
Duplicate `id` → the later block is rejected as a bad block. At most 64 blocks are read.

## State and transitions

None. Pages are opened and closed; the only transient state is the reading position of each text
page (index of the top paragraph), kept while the page is on the stack and across rotation.
