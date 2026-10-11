#pragma once
// Consistency rules between licenses/manifest.txt, NOTICE, third_party/, the CMake dependencies and the license
// files (spec 003 FR-012, FR-013; specs/003-about-box/contracts/manifest-format.md). Test code: the rules are a
// function over plain data so each one can be proven to fire on in-memory input, and are then run on the real
// repository by test_license_consistency.

#include <algorithm>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "pbnotes/about.h"

namespace licensecheck {

struct Inputs {
    pbnotes::about::Manifest manifest;
    std::string notice;                         // contents of NOTICE
    std::vector<std::string> third_party_dirs;  // names of the directories directly under third_party/
    std::vector<std::string> cmake_names;       // dependency names scraped from the CMake files
    std::function<long(const std::string&)> size_of;  // repo-relative path -> size; -1 missing, 0 for a directory
    std::string license_file;                   // contents of LICENSE
};

inline std::vector<std::string> lines_of(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(line);
    }
    return out;
}

// Heading lines of the NOTICE sections: the first non-empty line after a line of 80 dashes.
inline std::vector<std::string> notice_headings(const std::string& notice) {
    std::vector<std::string> headings;
    const auto lines = lines_of(notice);
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].size() == 80 && lines[i].find_first_not_of('-') == std::string::npos) {
            size_t j = i + 1;
            while (j < lines.size() && lines[j].empty()) ++j;
            if (j < lines.size()) headings.push_back(lines[j]);
        }
    }
    return headings;
}

// Dependency names a CMake file declares: find_package(<Name> ...) and the libraries of target_link_libraries
// (without the target, the keywords, variables, the project's own targets, pthread and m; "A::B" counts as "A").
inline std::vector<std::string> scrape_cmake_names(const std::string& cmake) {
    std::vector<std::string> out;
    auto words_of = [](const std::string& call) {
        std::vector<std::string> w;
        std::istringstream in(call);
        std::string t;
        while (in >> t) w.push_back(t);
        return w;
    };
    auto call_args = [&](const std::string& fn) {
        std::vector<std::vector<std::string>> calls;
        size_t pos = 0;
        while ((pos = cmake.find(fn + "(", pos)) != std::string::npos) {
            if (pos > 0 && (std::isalnum(static_cast<unsigned char>(cmake[pos - 1])) || cmake[pos - 1] == '_')) { pos += fn.size(); continue; }
            size_t close = cmake.find(')', pos);
            if (close == std::string::npos) break;
            calls.push_back(words_of(cmake.substr(pos + fn.size() + 1, close - pos - fn.size() - 1)));
            pos = close;
        }
        return calls;
    };
    for (const auto& w : call_args("find_package")) {
        if (!w.empty()) out.push_back(w[0]);
    }
    const std::set<std::string> keywords = {"PRIVATE", "PUBLIC", "INTERFACE", "pthread", "m", "notes_core"};
    for (const auto& w : call_args("target_link_libraries")) {
        for (size_t i = 1; i < w.size(); ++i) {
            const std::string& t = w[i];
            if (keywords.count(t) || t.compare(0, 2, "${") == 0 || t.compare(0, 8, "pbnotes_") == 0) continue;
            size_t colons = t.find("::");
            out.push_back(colons == std::string::npos ? t : t.substr(0, colons));
        }
    }
    return out;
}

inline std::string first_copyright_line(const std::string& license_file) {
    for (const std::string& line : lines_of(license_file)) {
        if (line.compare(0, 9, "Copyright") == 0) return line;
    }
    return "";
}

inline bool starts_with(const std::string& s, const std::string& prefix) { return s.compare(0, prefix.size(), prefix) == 0; }

// Findings, each starting with "rule N:" and naming the offending entry; empty when everything agrees.
inline std::vector<std::string> check_consistency(const Inputs& in) {
    std::vector<std::string> f;
    const auto& m = in.manifest;
    const std::string kind = "";

    // rule 6: the manifest parses cleanly
    if (!m.readable) f.push_back("rule 6: manifest is not readable");
    if (!m.app) f.push_back("rule 6: manifest has no valid app block");
    if (m.bad_blocks != 0) f.push_back("rule 6: manifest has " + std::to_string(m.bad_blocks) + " bad block(s)");

    // rule 1: components <-> NOTICE sections
    const auto headings = notice_headings(in.notice);
    for (const auto& c : m.components) {
        bool found = std::any_of(headings.begin(), headings.end(), [&](const std::string& h) { return starts_with(h, c.name); });
        if (!found) f.push_back("rule 1: no NOTICE section for '" + c.name + "'");
    }
    for (const std::string& h : headings) {
        bool found = std::any_of(m.components.begin(), m.components.end(),
                                 [&](const pbnotes::about::Entry& c) { return starts_with(h, c.name); });
        if (!found) f.push_back("rule 1: NOTICE section '" + h + "' has no manifest entry");
    }

    // rule 2: third_party/ directories are claimed
    std::set<std::string> sources;
    std::set<std::string> cmake_claimed;
    for (const auto& c : m.components) {
        sources.insert(c.source.begin(), c.source.end());
        cmake_claimed.insert(c.cmake.begin(), c.cmake.end());
    }
    for (const std::string& d : in.third_party_dirs) {
        if (!sources.count("third_party/" + d)) f.push_back("rule 2: third_party/" + d + " is not claimed by any entry");
    }

    // rule 3: CMake dependencies are claimed
    std::set<std::string> reported;
    for (const std::string& n : in.cmake_names) {
        if (!cmake_claimed.count(n) && reported.insert(n).second) f.push_back("rule 3: CMake dependency '" + n + "' is not claimed by any entry");
    }

    // rule 4: named files exist; texts are non-empty and within the cap
    auto check_entry_files = [&](const pbnotes::about::Entry& e) {
        for (const std::string& t : e.text) {
            long size = in.size_of(t);
            if (size < 0) f.push_back("rule 4: '" + e.name + "': text file missing: " + t);
            else if (size == 0) f.push_back("rule 4: '" + e.name + "': text file empty: " + t);
            else if (size > 256 * 1024) f.push_back("rule 4: '" + e.name + "': text file over 256 KiB: " + t);
        }
        for (const std::string& s : e.source) {
            if (in.size_of(s) < 0) f.push_back("rule 4: '" + e.name + "': source path missing: " + s);
        }
    };
    if (m.app) check_entry_files(*m.app);
    for (const auto& c : m.components) check_entry_files(c);

    // rule 5: the app's copyright line is the first Copyright line of LICENSE
    if (m.app) {
        const std::string expected = first_copyright_line(in.license_file);
        if (expected.empty()) f.push_back("rule 5: LICENSE has no Copyright line");
        else if (m.app->copyright != expected)
            f.push_back("rule 5: app copyright '" + m.app->copyright + "' differs from LICENSE '" + expected + "'");
    }
    return f;
}

}  // namespace licensecheck
