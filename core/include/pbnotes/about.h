#pragma once
// About box (spec 003): the manifest of the app and its third-party components, as read from
// licenses/manifest.txt. Qt-free; read-only file access. Format: specs/003-about-box/contracts/manifest-format.md.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pbnotes {
namespace about {

enum class Origin { Bundled, System };

// One manifest block that passed validation.
struct Entry {
    std::string id, name, purpose, license, holder, copyright, url;
    Origin origin = Origin::Bundled;       // components only
    std::vector<std::string> text;         // relative paths below the licenses root
    std::vector<std::string> source, cmake;  // used by the consistency test only
};

struct Manifest {
    bool readable = false;          // false: file missing, unreadable, or over 64 KiB
    std::optional<Entry> app;       // the `kind: app` block, if valid
    std::vector<Entry> components;  // valid components in file order
    int bad_blocks = 0;             // blocks dropped (syntax, missing key, duplicate id, bad path, ...)
};

// Pure. Applies every rule of the manifest format: at most 64 KiB, 64 blocks, field limits, path rules.
Manifest parse_manifest(std::string_view content);

// Reads <licenses_root>/licenses/manifest.txt. Logs the number of dropped blocks (count only).
Manifest load_manifest(const std::string& licenses_root);

struct LicenseText {
    bool available = false;               // at least one file was read
    bool truncated = false;               // a file was cut at 256 KiB
    std::vector<std::string> paragraphs;  // with a heading paragraph before each file when there are several
};

// Reads the files named by `entry.text` below `licenses_root`. A path that is empty, absolute, contains "..", a
// backslash or NUL, or whose real path (after symbolic links) leaves the root, is unreadable, like a missing,
// empty or non-regular file. Files over 256 KiB are cut on a line boundary and end with "Text shortened.".
LicenseText load_text(const std::string& licenses_root, const Entry& entry);

// Pure: splits at blank lines, keeps line breaks inside a paragraph, normalises CRLF, trims trailing blanks,
// drops leading/trailing blank lines, caps at 5,000 paragraphs.
std::vector<std::string> split_paragraphs(std::string_view text);

// Pure: "" and the build's "0.0.0" fallback are shown as "unknown"; anything else unchanged.
std::string display_version(std::string_view version);

}  // namespace about
}  // namespace pbnotes
