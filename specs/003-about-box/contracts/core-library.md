# Contract: core library (Qt-free) — `pbnotes/about.h`

New module in `notes_core` (no Qt, no InkView, no network, no writes). Names are indicative; the
behaviour is the contract. All functions are pure or read-only file access.

```cpp
namespace pbnotes::about {

enum class Origin { Bundled, System };

struct Entry {                       // one manifest block that passed validation
    std::string id, name, purpose, license, holder, copyright, url;
    Origin origin = Origin::Bundled;           // components only
    std::vector<std::string> text;             // relative paths below the licenses root
    std::vector<std::string> source, cmake;    // consistency test only
};

struct Manifest {
    bool readable = false;           // false: file missing, unreadable, or over 64 KiB
    std::optional<Entry> app;        // the `kind: app` block, if valid
    std::vector<Entry> components;   // valid components in file order
    int bad_blocks = 0;              // blocks dropped (syntax, missing key, duplicate id, bad path…)
};

Manifest parse_manifest(std::string_view content);           // pure; applies every rule of
                                                             // contracts/manifest-format.md
Manifest load_manifest(const std::string& licenses_root);    // reads <root>/licenses/manifest.txt

struct LicenseText {
    bool available = false;          // at least one file was read
    bool truncated = false;          // a file was cut at 256 KiB
    std::vector<std::string> paragraphs;   // headings between several files included
};

// Reads the files of `entry.text` below `licenses_root`. Rejects (treats as unreadable) any path
// that is absolute, contains "..", a backslash or NUL, or whose real path leaves the root.
LicenseText load_text(const std::string& licenses_root, const Entry& entry);

// Pure: splits at blank lines, keeps line breaks inside a paragraph, trims trailing spaces,
// drops leading/trailing blank lines, caps at 5,000 paragraphs.
std::vector<std::string> split_paragraphs(std::string_view text);

// Pure: "" and "0.0.0" → "unknown"; anything else unchanged.
std::string display_version(std::string_view version);

}  // namespace pbnotes::about
```

## Behaviour

| Function | Input | Result |
|----------|-------|--------|
| `parse_manifest` | well-formed manifest | `readable`, `app` set, `components` in order, `bad_blocks == 0` |
| | a block with a missing required key | block dropped, `bad_blocks` +1, the rest parsed |
| | duplicate `id` / second `kind: app` | later block dropped, counted |
| | `text: ../x`, `/etc/passwd`, `a\b` | block dropped, counted (path rules) |
| | unknown key | ignored |
| | CRLF line ends / BOM | parsed as LF / BOM skipped |
| | empty input | `readable == true`, no app, no components, `bad_blocks == 0` |
| | > 64 KiB | `readable == false` |
| | > 64 blocks | the first 64 are read |
| `load_manifest` | file missing | `readable == false` |
| `load_text` | all files readable | `available`, paragraphs of all files in order, heading paragraph (file base name) before each file when there are several |
| | some files unreadable | `available` if at least one was read; the unreadable ones are skipped |
| | none readable / `text` empty | `available == false` (the caller decides whether that is an error: empty `text` is not) |
| | file > 256 KiB | cut at the cap on a line boundary, `truncated`, last paragraph "Text shortened." |
| | path escaping the root (also via symlink) | treated as unreadable |
| `split_paragraphs` | `"a\nb\n\n\nc"` | `["a\nb", "c"]` |
| | `"\n\n"` / `""` | `[]` |
| | CRLF | normalised to LF |
| `display_version` | `""`, `"0.0.0"` | `"unknown"` |
| | `"0ae4a86"`, `"0.2.0-3-gabc123-dirty"` | unchanged |

## Logging

`about.opened` (event, no fields), `about.bad_blocks` and `about.text_unavailable` (counts only).
No path, file name, component name or license text is logged (R6 of `check-constitution.sh`).

## Tests (written first)

`test_about_manifest` (rows above), `test_about_text` (`load_text`, `split_paragraphs`, path
rules, size cap, unreadable file), `test_about_version`, `test_about_files` (real tree),
`test_license_consistency` (consistency rules with negative cases, helper in
`tests/support/license_check.h`).
