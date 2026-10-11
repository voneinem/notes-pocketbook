#include "pbnotes/edit_history.h"

#include <algorithm>

namespace pbnotes {

namespace {

constexpr int64_t kMergeGapMs = 1500;
constexpr size_t kMergeMaxUnits = 80;

bool is_high(char16_t c) { return c >= 0xD800 && c <= 0xDBFF; }
bool is_low(char16_t c) { return c >= 0xDC00 && c <= 0xDFFF; }

bool text_field(EditField f) { return f == EditField::Title || f == EditField::Text; }

// One code point's worth of units: 1, or 2 for a surrogate pair.
bool is_single_char(std::u16string_view s) {
    return s.size() == 1 || (s.size() == 2 && is_high(s[0]) && is_low(s[1]));
}

// True when replacing `rem` units at `pos` of `before` by `ins` gives `after`.
bool replaces(std::u16string_view before, std::u16string_view after, size_t pos, size_t rem, std::u16string_view ins) {
    return pos + rem <= before.size() && after.size() == before.size() - rem + ins.size() &&
           after.substr(0, pos) == before.substr(0, pos) && after.substr(pos, ins.size()) == ins &&
           after.substr(pos + ins.size()) == before.substr(pos + rem);
}

struct Diff {
    size_t pos = 0;
    std::u16string removed, inserted;
};

// The minimal replacement turning `before` into `after`: common prefix, then common suffix, neither splitting a
// surrogate pair.
Diff diff(std::u16string_view before, std::u16string_view after) {
    size_t p = 0;
    const size_t limit = std::min(before.size(), after.size());
    while (p < limit && before[p] == after[p]) ++p;
    if (p > 0 && is_high(before[p - 1])) --p;  // the pair's low unit differs or is missing
    size_t s = 0;
    while (s < limit - p && before[before.size() - 1 - s] == after[after.size() - 1 - s]) ++s;
    if (s > 0 && is_low(before[before.size() - s])) --s;  // the pair's high unit is outside the suffix
    Diff d;
    d.pos = p;
    d.removed.assign(before.substr(p, before.size() - p - s));
    d.inserted.assign(after.substr(p, after.size() - p - s));
    return d;
}

}  // namespace

EditHistory::EditHistory(size_t max_steps, size_t max_chars) : max_steps_(max_steps), max_chars_(max_chars) {}

void EditHistory::reset(std::u16string title, std::u16string text, std::u16string category, bool favorite) {
    current_[0] = std::move(title);
    current_[1] = std::move(text);
    current_[2] = std::move(category);
    current_[3] = favorite ? u"1" : u"";
    undo_.clear();
    redo_.clear();
    chars_ = 0;
    grouping_ = false;
    applying_ = false;
    active_ = true;
}

void EditHistory::observe(EditField field, std::u16string_view value, int64_t now_ms) {
    if (!active_ || applying_) return;
    std::u16string& cur = known(field);
    if (value == cur) return;
    if (grouping_ && field == group_field_) {  // the group's step is made at end_group()
        cur.assign(value);
        group_ms_ = now_ms;
        return;
    }
    Step step;
    step.field = field;
    step.last_ms = now_ms;
    if (text_field(field)) {
        Diff d = diff(cur, value);
        step.pos = d.pos;
        step.removed = std::move(d.removed);
        step.inserted = std::move(d.inserted);
        if (step.removed.empty() && is_single_char(step.inserted) && step.inserted != u"\n") step.kind = Kind::Typing;
        else if (step.inserted.empty() && is_single_char(step.removed)) step.kind = Kind::Removal;
    } else {
        step.removed = cur;
        step.inserted.assign(value);
    }
    push(std::move(step), true, cur, value);
    cur.assign(value);
}

void EditHistory::push(Step step, bool mergeable, std::u16string_view before, std::u16string_view after) {
    redo_.clear();
    if (mergeable && !undo_.empty()) {
        Step& prev = undo_.back();
        if (prev.field == step.field && text_field(step.field) && step.last_ms - prev.last_ms <= kMergeGapMs &&
            step.last_ms >= prev.last_ms) {
            if (step.kind == Kind::Typing && prev.kind == Kind::Typing &&
                prev.inserted.size() + step.inserted.size() <= kMergeMaxUnits &&
                replaces(before, after, prev.pos + prev.inserted.size(), 0, step.inserted)) {
                prev.inserted += step.inserted;
                prev.last_ms = step.last_ms;
                chars_ += step.inserted.size();
                trim();
                return;
            }
            if (step.kind == Kind::Removal && prev.removed.size() + step.removed.size() <= kMergeMaxUnits) {
                const size_t len = step.removed.size();
                const bool backwards = (prev.kind == Kind::Removal || prev.kind == Kind::Backspace) && prev.pos >= len &&
                                       replaces(before, after, prev.pos - len, len, u"");
                const bool forwards = !backwards && (prev.kind == Kind::Removal || prev.kind == Kind::Delete) &&
                                      replaces(before, after, prev.pos, len, u"");
                if (backwards) {
                    prev.pos -= len;
                    prev.removed = std::u16string(before.substr(prev.pos, len)) + prev.removed;
                    prev.kind = Kind::Backspace;
                } else if (forwards) {
                    prev.removed += before.substr(prev.pos, len);
                    prev.kind = Kind::Delete;
                }
                if (backwards || forwards) {
                    prev.last_ms = step.last_ms;
                    chars_ += len;
                    trim();
                    return;
                }
            }
        }
    }
    chars_ += step.removed.size() + step.inserted.size();
    undo_.push_back(std::move(step));
    trim();
}

void EditHistory::trim() {
    while (undo_.size() > 1 && (undo_.size() > max_steps_ || chars_ > max_chars_)) {
        chars_ -= undo_.front().removed.size() + undo_.front().inserted.size();
        undo_.pop_front();
    }
}

void EditHistory::begin_group(EditField field) {
    if (!active_) return;
    if (grouping_) end_group();
    grouping_ = true;
    group_field_ = field;
    group_start_ = known(field);
    group_ms_ = 0;
}

void EditHistory::end_group() {
    if (!grouping_) return;
    grouping_ = false;
    std::u16string& cur = known(group_field_);
    if (cur == group_start_) return;
    Step step;
    step.field = group_field_;
    step.last_ms = group_ms_;
    if (text_field(group_field_)) {
        Diff d = diff(group_start_, cur);
        step.pos = d.pos;
        step.removed = std::move(d.removed);
        step.inserted = std::move(d.inserted);
    } else {
        step.removed = group_start_;
        step.inserted = cur;
    }
    push(std::move(step), false, std::u16string_view(), std::u16string_view());
}

EditApplied EditHistory::apply(const Step& step, bool undoing) {
    EditApplied a;
    a.field = step.field;
    const std::u16string& from = undoing ? step.inserted : step.removed;  // what is there now
    const std::u16string& to = undoing ? step.removed : step.inserted;    // what it becomes
    if (text_field(step.field)) {
        a.pos = step.pos;
        a.remove_len = from.size();
        a.insert = to;
        known(step.field).replace(step.pos, from.size(), to);
        if (undoing && !to.empty()) {  // the restored text is selected
            a.sel_begin = step.pos;
            a.sel_end = step.pos + to.size();
        } else {
            a.sel_begin = a.sel_end = undoing ? step.pos : step.pos + to.size();
        }
    } else {
        a.value = to;
        known(step.field) = to;
    }
    return a;
}

std::optional<EditApplied> EditHistory::undo() {
    if (!active_) return std::nullopt;
    end_group();  // an open group (a typed "/" waiting for its menu) becomes a step first, so it can be undone
    if (undo_.empty()) return std::nullopt;
    Step step = std::move(undo_.back());
    undo_.pop_back();
    chars_ -= step.removed.size() + step.inserted.size();
    applying_ = true;
    EditApplied a = apply(step, true);
    redo_.push_back(std::move(step));
    return a;
}

std::optional<EditApplied> EditHistory::redo() {
    if (!active_ || redo_.empty()) return std::nullopt;
    Step step = std::move(redo_.back());
    redo_.pop_back();
    applying_ = true;
    EditApplied a = apply(step, false);
    chars_ += step.removed.size() + step.inserted.size();
    undo_.push_back(std::move(step));
    return a;
}

void EditHistory::finish_apply(std::u16string_view title, std::u16string_view text) {
    applying_ = false;
    current_[0].assign(title);
    current_[1].assign(text);
}

}  // namespace pbnotes
