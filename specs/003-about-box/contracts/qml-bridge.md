# Contract: Qt bridge and QML pages

## `AboutBridge` (QObject, context property `about`)

Read-only. Created in `app/main.cpp` with the licenses root and `pbnotes::version()`. Reads the
manifest when first asked and caches it for the process; reads a text only when its page is opened.
Does not touch the store, the settings or the network.

| Member | Type | Meaning |
|--------|------|---------|
| `appName` | string | from the build / manifest app block |
| `version` | string | `display_version(pbnotes::version())` |
| `author` | string | app block `holder`; `""` if unavailable |
| `copyright` | string | app block `copyright`; `""` if unavailable |
| `licenseName` | string | app block `license`; `""` if unavailable |
| `infoAvailable` | bool | false when the app block is missing (the page shows the unavailable marker for `author`, `copyright`, `licenseName`) |
| `components` | list of `{id, name, purpose, license, origin}` | valid components, `bundled` first then `system`, manifest order inside each group |
| `unreadableEntries` | int | `bad_blocks` (the list page shows "N entries could not be read." when > 0) |
| `appLicense()` | `{title, header, paragraphs, available}` | the app license page content |
| `componentDetail(id)` | `{title, header, paragraphs, available}` or `{}` for an unknown id | one component's page |

`header` is a list of `"Label: value"` lines (holder, license, origin, address, purpose). For a
component with an empty `text` list, `paragraphs` is a single line "The full license text is
published at <url>." and `available` is true. If a component has text files but none could be
read, `available` is false and `paragraphs` is a single line "The license text could not be read."
(the header lines are still given). If the truncation cap applied, the last paragraph is "Text
shortened.".

No signals; the data does not change while the app runs. Opening the pages writes nothing and
starts no network request.

## Pages (all new, in `qml/pages/`)

### `AboutPage.qml`

- Title "About"; app name (large), "Version <version>", "Author: <author>", the copyright line,
  "License: <licenseName>". Any missing item shows "unavailable".
- Buttons: "License" (pushes `LicenseTextPage` with `about.appLicense()`), "Third-party licenses"
  (pushes `ThirdPartyPage`), "Back" (pops, returning to Settings).
- Rotation: layout follows the window; scrolls with `PageButtons` when it does not fit.

### `ThirdPartyPage.qml`

- Title "Third-party licenses"; a `ListView` of fixed-height rows, each: **name** (bold), purpose (one line),
  "<license> | <In the app package | Provided by the reader>". A row is tappable and pushes `LicenseTextPage` with
  `about.componentDetail(id)`. The model order already puts `bundled` entries first; the origin is named in every
  row instead of in separate group labels, so that rows have one height and page breaks fall on row edges.
- A line "N entries could not be read." under the title when `unreadableEntries > 0`; the text "The list of
  third-party licenses is unavailable." when the list is empty.
- `PageButtons` with `snap` (whole rows), "Back".

### `LicenseTextPage.qml`

- Input: `{title, header, paragraphs, available}`.
- The `header` lines first (bold labels), then the paragraphs in a `ListView` (26 px, black on
  white, `PlainText`), `PageButtons`, "Back". When `available` is false the page shows the header
  and the single message paragraph.
- Rotation: the index of the top visible paragraph is remembered and the view is repositioned to it
  after a size change (FR-015).

### `SettingsPage.qml` (changed)

One new flat button "About" in its own section after "Account", pushing `AboutPage`. Nothing else on
the page changes.

## Conventions kept

Black on white, flat `InkButton`s, no animation, paged scrolling only, `qsTr` for every UI string
(the licence texts and the manifest strings are shown as read, in their original language).
`qml.qrc` lists the three new files; the app `--self-test` must load them without QML errors.

## Tests (written first)

`test_about_bridge` (offscreen Qt, fixture licenses root): properties with a good manifest; with no
manifest (`infoAvailable == false`, empty list, no crash); a component with and without text; a
component whose text file is missing; unknown id; ordering by origin; data directory unchanged
before and after calling every member. The app `--self-test` covers the QML.
