# Research: About Box (Version, Author, License, Third-Party Licenses)

Resolves what the plan's Technical Context left open. Facts were read from this repository and from
the local PocketBook SDK (headers, `readelf`); nothing was probed on the device. Items marked
**[VERIFY]** need a check during implementation and have a stated fallback; none blocks the design.

## R-1. Which components are "third-party components" (FR-008)

- **Findings**: `readelf -d` on the ARM build lists the shared libraries the app needs:
  `libQt6Quick`, `libQt6Qml`, `libQt6Gui`, `libQt6Core`, `libinkview`, `libcurl`, `libstdc++`,
  `libgcc_s`, `libm`, `libc`. OpenSSL is not linked by the app itself, but `libcurl` performs every
  TLS connection with it (SDK: OpenSSL 3.5.4). The package ships four more third-party items that
  are not libraries: InkFeed-derived code (compiled in), the Mozilla CA bundle, SQLite (compiled
  in), and the Noto Emoji font. `Qt6Network` and `Qt6Concurrent` are no longer linked.
- **Decision**: a component is listed when it is (a) compiled into or shipped with the app, or (b)
  named as needed by the app's binary, or (c) performs a function the app relies on without being
  linked by it (OpenSSL for TLS). The list therefore has 10 entries (see R-2) plus the app itself.
  The system libraries are marked "provided by the reader" so a reader can tell what travels in the
  package.
- **Alternatives considered**: list only what the package ships (misses Qt and curl, which the
  spec's FR-008 explicitly includes); list the full transitive closure of the device's libraries
  (ICU, libpng, libjpeg, …; unbounded, changes with firmware, and not the app's choice).

## R-2. Component list and licenses

| Entry | License | Origin | Full text for the app to show |
|-------|---------|--------|-------------------------------|
| Notes Pocketbook (the app) | MIT | — | `LICENSE` (repository root) |
| InkFeed (build, launcher and packaging patterns) | MIT, © 2026 Joost Rohde | bundled | its MIT text (today only inside `NOTICE`) |
| Mozilla CA certificate bundle | MPL-2.0 | bundled | MPL-2.0 text |
| SQLite | Public domain | bundled | none exists; entry states the dedication and gives https://www.sqlite.org/copyright.html |
| Noto Emoji (font) | SIL OFL 1.1, © 2013 Google LLC | bundled | `third_party/noto-emoji/OFL.txt` |
| Qt 6.8 (Core, Gui, Qml, Quick) | LGPL-3.0 (Qt headers: `LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only OR commercial`) | system | LGPL-3.0 + GPL-3.0 texts |
| libcurl | curl license (MIT-style), © Daniel Stenberg et al. | system | curl `COPYING` text |
| OpenSSL 3.5 | Apache-2.0 | system | Apache-2.0 text |
| PocketBook InkView (`libinkview`) | PocketBook's SDK terms (proprietary) **[VERIFY]** | system | none; entry gives name, holder and the address of PocketBook's SDK page **[VERIFY]** |
| GNU C Library (`libc`, `libm`) | LGPL-2.1-or-later | system | LGPL-2.1 text |
| GCC runtime libraries (`libstdc++`, `libgcc_s`) | GPL-3.0-or-later with GCC Runtime Library Exception 3.1 | system | GPL-3.0 text; exception by address **[VERIFY]** |

- **Sources of the Qt and curl facts**: the SDK's Qt headers carry the `SPDX-License-Identifier`
  line quoted above; the SDK's curl headers name Daniel Stenberg as copyright holder and point to
  `COPYING`. The curl text was read from the copy that accompanies the SDK's CMake sources; its
  wording is the curl license (year range of the copy to be taken from the copy used).
- **[VERIFY] InkView terms**: `inkview.h` carries no license header and the SDK ships no license
  file for it. The entry must say only what is known (a library of the reader's firmware, not
  redistributed by this project, terms set by PocketBook) and must not claim a license name. If the
  SDK's repository states terms, use them verbatim.
- **Decision**: the license *texts* come from the standard texts available offline in the
  maintainers' environment (`/usr/share/common-licenses` on Debian/Ubuntu for MPL-2.0, Apache-2.0,
  LGPL-2.1, LGPL-3, GPL-3) and the project's own files; their provenance is recorded in
  `licenses/SOURCES.md`. Where a full text does not exist or is not distributed with the component,
  FR-010's fallback applies: name, holder, license name and published address.
- **Alternatives considered**: link to the license texts online only (violates "readable offline",
  FR-005/SC-003); ship only the license names (the point of the screen is the text); include the
  system libraries' texts only in `NOTICE` (a reader with only the device would not see them).

## R-3. Where the single source of truth lives (FR-012, FR-013)

- **Findings**: `NOTICE` is free text (long license bodies, prose). Parsing it for names, purposes
  and license names would need a convention that the free text cannot carry (a "purpose" has no
  place). A generated `NOTICE` would add a generator and a generated file in git.
- **Decision**: a hand-edited, line-based **manifest** `licenses/manifest.txt` is the one place
  where a component is declared (name, purpose, license, holder, address, origin, text files, and
  the repository/build evidence that proves it exists). The app reads it at run time. `NOTICE`
  stays hand-written; a **host consistency test** compares the two plus the build evidence:
  - every non-app manifest entry has a `NOTICE` section whose heading starts with the entry's name,
    and every `NOTICE` section has a manifest entry;
  - every directory under `third_party/` is claimed by an entry (`source:`), so a vendored library
    cannot be added silently;
  - every `find_package(...)` name and linked library in the CMake files that is not a build-only
    tool is claimed by an entry (`cmake:`), so a new dependency cannot be added silently;
  - every text file named exists, is non-empty and is within the size cap;
  - the app entry's copyright line equals the first `Copyright` line of `LICENSE`.
  The test runs under CTest, so `scripts/pre-merge.sh` and every build run it (FR-013).
- **Alternatives considered**: JSON manifest (escaping hurts hand editing and review diffs of long
  strings, and gives nothing the block format lacks); hard-coding the list in QML or C++ (cannot be
  checked against `NOTICE`, drifts); deriving everything from `NOTICE` (see above); a generator
  producing both (extra tool and generated artifact for 12 entries — YAGNI).

## R-4. Manifest format

- **Decision**: UTF-8 text, blocks separated by one or more blank lines, one `key: value` per line,
  `#` comment lines ignored, list values comma-separated. Keys: `id`, `kind` (`app` or
  `component`), `name`, `purpose`, `license`, `holder`, `url`, `origin` (`bundled` or `system`),
  `text` (list of paths), `source` (list, repository paths), `cmake` (list). The full grammar,
  limits and error rules are in `contracts/manifest-format.md`.
- **Validation (constitution III, external input)**: the manifest and every text are files that sit
  next to the app on a user-writable volume, so they are treated as input: size caps (manifest 64
  KiB, each text 256 KiB), `text` paths must be relative, contain no `..` and no backslash, and are
  resolved only below the licenses root; unknown keys are ignored; a bad block is skipped and
  reported as one "unavailable" entry rather than failing the screen (FR-016).
- **Alternatives considered**: INI/TOML (needs a parser or dependency); reusing `json.h` (workable,
  rejected above); `.desktop`-style files, one per component (11 small files to keep in step).

## R-5. Shipping the files on the device and reading them on the host

- **Findings**: the package already installs `pbnotes-cacert.pem`, `pbnotes-emoji.ttf` and
  `pbnotes-emoji-OFL.txt` flat into `/mnt/ext1/applications/`. The app finds them by absolute path
  on the device and through a compile-time definition on the host.
- **Decision**: a directory `pbnotes-licenses/` next to the binary holds `manifest.txt` and every
  referenced text **at the same relative path it has in the repository** (`licenses/…`, `LICENSE`,
  `third_party/noto-emoji/OFL.txt`). The licenses root is that directory on the device and the
  repository root on the host (`PBNOTES_LICENSES_ROOT` definition, overridable by environment
  variable like `PBNOTES_EMOJI_FONT`). No path mapping code, no duplicated `LICENSE`, no symlinks,
  and `package-device.sh` copies exactly the files the manifest names (`install -D`), so a missing
  file fails packaging. The existing flat `pbnotes-emoji-OFL.txt` stays (its OFL clause wants the
  license installed with the font).
- **Alternatives considered**: embed the texts in the QML resource file (about 130 KiB into the
  binary; the host test could not diff them against the sources and changing a text means
  rebuilding); flat directory with renamed copies (needs a name map and a collision rule);
  symlinks in git (not portable).

## R-6. Version string (FR-003, SC-004)

- **Findings**: the build first computed `git describe --tags --always --dirty` (e.g. `0ae4a86`,
  `v0.2.0-3-gabc123-dirty`), so an untagged or modified tree showed a commit hash or `-dirty`.
  The packaging script derived the version again to name the zip, and the two could disagree
  (`v` prefix, `-dirty`), which breaks SC-004.
- **Decision (revised after the first device install)**: the release version is the first line of the
  committed `VERSION` file (`1.0.0`). `CMakeLists.txt` reads it (also for `project(VERSION)`), and
  `package-device.sh` reads the same file to name the zip. `-DPBNOTES_VERSION=...` still overrides the
  build. As a guard `package-device.sh` fails if the built ELF does not contain its version string.
  The About screen shows `pbnotes::version()` as is; `display_version()` turns an empty string or
  `0.0.0` into "unknown". A release is: edit `VERSION`, commit, tag `v<version>`.
- **Alternatives considered**: deriving from `git describe` (shows hashes and `-dirty` on untagged or
  modified trees; needs a tag before every build); injecting the version at package time with `objcopy` (patches a built binary; the guard is
  simpler).

## R-7. Showing long text on the screen (FR-007, FR-014, FR-015)

- **Findings**: the longest text is GPL-3.0 (35 KB, about 620 lines). The reader screen already
  pages through a `ListView` of blocks with `PageButtons`; a single `Text` item of that height risks
  oversized rendering textures with some Qt render types, and a `ListView` of paragraphs is the
  form the app already uses and tests.
- **Decision**: the core splits a text into paragraphs (blank-line separated, original line breaks
  inside a paragraph kept, trailing spaces trimmed, tabs kept) and the page shows them in a
  `ListView` with `PageButtons`, 26 px text. Line breaks inside paragraphs are kept because the
  standard texts rely on indentation and numbered clauses; hard-wrapped lines of about 80 characters
  fit the portrait width at 26 px, and a longer line wraps cosmetically.
  Rotation keeps the reader's place by remembering the index of the top paragraph and repositioning
  after the size change.
- **Alternatives considered**: one `Text` per license (texture risk, no row-snapped paging);
  re-flowing paragraphs by joining lines (breaks indented lists and the GPL's layout); a monospaced
  font (wider, more wrapping).

## R-8. Navigation and screens

- **Decision**: Settings gets one new button "About" (below Account, above or next to the existing
  items) that pushes `AboutPage`. `AboutPage` shows name, version, author, copyright, license and
  two buttons: "License" and "Third-party licenses". "License" and each list row push
  `LicenseTextPage` (a header block plus the paged paragraphs); the list is `ThirdPartyPage`. Three
  new QML files, no new dialog type, same `InkButton`, black on white, no animation. Back is the
  existing Back control of the stack (as on Settings).
- **Reach (SC-001)**: notes list → Settings → About is 2 taps; version visible on arrival.
- **Alternatives considered**: a modal dialog (would not fit 600 lines or a list, and rotation
  handling of dialogs is already a source of work); About as the first item of the notes list
  (clutters the main screen); showing the version in the Settings footer instead (does not satisfy
  author/license).

## R-9. Logging and privacy

- **Decision**: no network call and no write. Logging uses counts and event names only (`about.opened`,
  `about.component_unavailable` with a count); the existing rule R6 of `check-constitution.sh`
  forbids log keys named like paths, files or names, so the code cannot log which file failed.
- **Alternatives considered**: logging the failing path (useful to a developer, forbidden by the
  project's logging rule; the unavailable entry on screen already tells the reader which component).

## R-10. Test plan overview (constitution I, IV, V)

- **Unit (host, Qt-free)**: `test_about_manifest` (parse, limits, bad blocks, path rules, unknown
  keys, duplicates), `test_about_text` (paragraph splitting, size cap, unreadable file),
  `test_about_version` (`display_version`), `test_license_consistency` (R-3 rules, with negative
  cases proving each rule fires, in the style of the `check-constitution.sh --self-test`).
- **Integration (device-boundary file format)**: `test_about_files` loads the real
  `licenses/manifest.txt` and every real text from the repository; this is the contract test for the
  manifest format and doubles as the check that the packaged tree is complete.
- **Bridge (offscreen Qt)**: `test_about_bridge` (properties, list, detail, unavailable path, no
  writes: the data directory is unchanged before/after).
- **App self-test**: the existing `pbnotes_app --self-test` must still load all QML with the three
  new pages.
- **Device (manual, quickstart §3)**: open About, both license screens, rotation, offline.
- **[VERIFY] items**: InkView terms (R-2); GCC Runtime Library Exception text availability (R-2);
  the year range of the curl license copy (R-2).

## Outcome of the [VERIFY] items (settled 2026-10-11, during implementation)

- **InkView terms**: no license file in the SDK; `inkview.h` and the headers under `inkview/` carry no
  copyright or license line. The manifest entry names no license (holder "PocketBook").
- **GCC Runtime Library Exception**: not present in the SDK files; the entry gives the published
  address https://www.gnu.org/licenses/gcc-exception-3.1.html and shows GPL-3.0.
- **curl license year range**: the copy used reads "1996 - 2024".
