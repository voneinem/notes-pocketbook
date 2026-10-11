// GitHub-style pipe tables (spec 002, research R-10).
#include <algorithm>
#include <cctype>

#include "markdown_internal.h"

namespace pbnotes {
namespace md {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && is_space(s[a])) ++a;
    while (b > a && is_space(s[b - 1])) --b;
    return s.substr(a, b - a);
}

// Splits a line into trimmed cells on unescaped pipes ("\|" is a literal pipe). A leading and a
// trailing pipe do not create empty cells. `*separators` is the number of unescaped pipes.
std::vector<std::string> split_cells(const std::string& line, int* separators) {
    const std::string t = trim(line);
    std::vector<std::string> cells(1);
    int seps = 0;
    bool last_was_sep = false;
    for (size_t i = 0; i < t.size(); ++i) {
        last_was_sep = false;
        if (t[i] == '\\' && i + 1 < t.size() && t[i + 1] == '|') {
            cells.back().push_back('|');
            ++i;
        } else if (t[i] == '|') {
            ++seps;
            cells.emplace_back();
            last_was_sep = (i + 1 == t.size());
        } else {
            cells.back().push_back(t[i]);
        }
    }
    if (!t.empty() && t[0] == '|') cells.erase(cells.begin());
    if (last_was_sep && !cells.empty()) cells.pop_back();
    for (auto& c : cells) c = trim(c);
    if (separators) *separators = seps;
    return cells;
}

// "---", ":--", "--:", ":-:" -> alignment; false if the cell is not a delimiter cell.
bool delimiter_cell(const std::string& c, TableData::Align* align) {
    if (c.empty()) return false;
    size_t a = 0, b = c.size();
    const bool left = c[a] == ':';
    if (left) ++a;
    const bool right = b > a && c[b - 1] == ':';
    if (right) --b;
    if (b <= a) return false;
    for (size_t i = a; i < b; ++i)
        if (c[i] != '-') return false;
    *align = left && right ? TableData::Center : left ? TableData::Left : right ? TableData::Right : TableData::None;
    return true;
}

// A line that cannot belong to a table body: another block starts there.
bool ends_table(const std::string& line) {
    const std::string t = trim(line);
    if (t.empty() || t[0] == '>') return true;
    if (open_fence(line)) return true;
    size_t h = 0;
    while (h < t.size() && t[h] == '#') ++h;
    return h >= 1 && h <= 6 && h < t.size() && t[h] == ' ';
}

size_t codepoints(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) ++n;
    return n;
}

}  // namespace

size_t read_table(const std::vector<std::string>& lines, size_t start, Block* out) {
    if (start + 1 >= lines.size()) return 0;
    int seps = 0;
    const std::vector<std::string> header = split_cells(lines[start], &seps);
    if (seps == 0 || header.empty()) return 0;

    int delim_seps = 0;
    const std::vector<std::string> delim = split_cells(lines[start + 1], &delim_seps);
    if (delim_seps == 0 || delim.size() != header.size()) return 0;
    std::vector<TableData::Align> align;
    for (const auto& c : delim) {
        TableData::Align a;
        if (!delimiter_cell(c, &a)) return 0;
        align.push_back(a);
    }

    std::vector<std::vector<std::string>> rows;
    rows.push_back(header);
    size_t used = 2;
    while (start + used < lines.size()) {
        const std::string& line = lines[start + used];
        int s = 0;
        if (ends_table(line) || line.size() > 16384) break;  // an overlong line is plain text (see markdown.cpp)
        std::vector<std::string> cells = split_cells(line, &s);
        if (s == 0) break;
        rows.push_back(std::move(cells));
        ++used;
    }

    size_t cols = 0;
    for (const auto& r : rows) cols = std::max(cols, r.size());
    align.resize(cols, TableData::None);

    TableData t;
    t.align = align;
    std::vector<size_t> widest(cols, 0);
    for (auto& r : rows) {
        r.resize(cols);
        std::vector<std::vector<Span>> cells;
        for (size_t c = 0; c < cols; ++c) {
            cells.push_back(cell_spans(r[c]));
            widest[c] = std::max(widest[c], codepoints(r[c]));
        }
        t.rows.push_back(std::move(cells));
    }
    // relative widths from the longest cell, clamped so no column vanishes or takes everything
    double sum = 0;
    for (size_t c = 0; c < cols; ++c) {
        const double w = static_cast<double>(std::min<size_t>(40, std::max<size_t>(4, widest[c])));
        t.weight.push_back(w);
        sum += w;
    }
    for (double& w : t.weight) w /= sum;

    out->kind = BlockKind::Table;
    out->line_count = static_cast<int>(used);
    out->table = std::move(t);
    return used;
}

}  // namespace md
}  // namespace pbnotes
