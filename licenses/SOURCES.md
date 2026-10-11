# Where the license texts come from

The About screen shows these files (named by `manifest.txt`). They are the standard published
texts, copied unchanged unless noted.

| File | Source |
|------|--------|
| `MPL-2.0.txt` | `common-licenses/MPL-2.0` of the Debian/Ubuntu `base-files` package |
| `Apache-2.0.txt` | `common-licenses/Apache-2.0` of the same package |
| `LGPL-2.1.txt` | `common-licenses/LGPL-2.1` of the same package |
| `LGPL-3.0.txt` | `common-licenses/LGPL-3` of the same package |
| `GPL-3.0.txt` | `common-licenses/GPL-3` of the same package |
| `curl.txt` | `COPYING` of the curl sources that accompany the PocketBook SDK's CMake (copyright line `1996 - 2024, Daniel Stenberg`); the device's libcurl is a newer release under the same license |
| `inkfeed-MIT.txt` | the InkFeed section of `NOTICE` (MIT, Copyright (c) 2026 Joost Rohde) |
| `../LICENSE` (named as `LICENSE`) | this project's own license |
| `../third_party/noto-emoji/OFL.txt` | Noto Emoji, see `third_party/noto-emoji/VERSION` |

## Components without a text file

- **SQLite** is in the public domain; there is no license text. The entry gives the address of the
  dedication.
- **PocketBook InkView** (`libinkview`): the SDK ships no license file and `inkview.h` and the other
  headers carry no copyright or license statement (checked 2026-10-11). The entry therefore names
  no license; it says the library is part of the reader's firmware, not redistributed by this
  project, and gives PocketBook's address (the host name appears in the library itself).
- **GCC runtime libraries**: the GCC Runtime Library Exception 3.1 text is not part of the SDK's
  files (checked 2026-10-11). The entry shows the GPL-3.0 text and gives the published address of
  the exception.
