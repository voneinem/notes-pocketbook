#pragma once
// The editor's undo/redo history (spec 005 FR-011..FR-017): one history over the title, the text, the category
// and the favorite marker. The editor reports each field's new value; the history diffs it against the value it
// knows, so typing, pasting, cutting and programmatic inserts are all captured the same way. No Qt.
//
// Offsets are UTF-16 code units (the unit Qt's text items use); a surrogate pair is never split. Values of the
// Favorite field are "1" (on) and "" (off).

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>

namespace pbnotes {

enum class EditField { Title = 0, Text = 1, Category = 2, Favorite = 3 };

// What the editor must do for an undo or redo.
struct EditApplied {
    EditField field = EditField::Text;
    size_t pos = 0;          // Title/Text: delete remove_len units at pos, then insert `insert` there
    size_t remove_len = 0;
    std::u16string insert;
    std::u16string value;    // Category/Favorite: the value to set
    size_t sel_begin = 0;    // select this range afterwards (equal = just a caret)
    size_t sel_end = 0;
};

class EditHistory {
public:
    explicit EditHistory(size_t max_steps = 200, size_t max_chars = 2000000);

    // Starts a history at the opened values; clears both stacks and any group or apply state. The opened
    // content is the baseline: no step can remove it.
    void reset(std::u16string title, std::u16string text, std::u16string category, bool favorite);
    bool active() const { return active_; }

    // Reports the field's new value. A no-op when it equals the known value, while applying, or before reset.
    // Single-character typing and single-character delete runs merge into the newest step (same field, adjacent,
    // at most 1500 ms apart, at most 80 units, no line break); everything else is a step of its own.
    void observe(EditField field, std::u16string_view value, int64_t now_ms);

    // A compound command (Cut, Delete, Paste over a selection): everything observed for `field` until end_group()
    // becomes at most one step. A second begin_group ends the first.
    void begin_group(EditField field);
    void end_group();

    bool can_undo() const { return !undo_.empty(); }
    bool can_redo() const { return !redo_.empty(); }
    // nullopt when there is nothing to undo/redo. Both set "applying": observe() is ignored until finish_apply.
    std::optional<EditApplied> undo();
    std::optional<EditApplied> redo();
    // Ends "applying" and re-syncs the known title/text with what the editor really holds.
    void finish_apply(std::u16string_view title, std::u16string_view text);

    size_t undo_size() const { return undo_.size(); }
    size_t redo_size() const { return redo_.size(); }

private:
    enum class Kind { Typing, Removal, Backspace, Delete, Other };
    struct Step {
        EditField field;
        Kind kind = Kind::Other;
        size_t pos = 0;
        std::u16string removed;   // what was there (Category/Favorite: the old value)
        std::u16string inserted;  // what is there now (Category/Favorite: the new value)
        int64_t last_ms = 0;
    };

    std::u16string& known(EditField f) { return current_[static_cast<size_t>(f)]; }
    // `before`/`after` are the field's values around the step; a diff of repeated characters can place an edit one
    // position off ("llo" -> "lo"), so merging re-checks adjacency against them.
    void push(Step step, bool mergeable, std::u16string_view before, std::u16string_view after);
    void trim();
    EditApplied apply(const Step& step, bool undoing);

    size_t max_steps_, max_chars_;
    bool active_ = false;
    bool applying_ = false;
    std::u16string current_[4];
    std::deque<Step> undo_, redo_;
    size_t chars_ = 0;  // stored characters in undo_

    bool grouping_ = false;
    EditField group_field_ = EditField::Text;
    std::u16string group_start_;
    int64_t group_ms_ = 0;
};

}  // namespace pbnotes
