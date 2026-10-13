# Contract: core library

Qt-free; builds on the host (constitution V). Extends the contracts of features 001–006.

## `pbnotes/sync_scheduler.h` (new)

```cpp
namespace pbnotes {

// Decides WHEN an automatic sync starts after a save. Holds no clock, no timer and no thread: the
// caller passes the time and the situation, and does what the answer says. Not thread-safe: use it
// from one thread (the bridge's, the UI thread).
class SyncScheduler {
public:
    struct Options {
        int64_t quiet_ms = 4000;   // no sync until this long after the latest save
        int64_t gap_ms = 15000;    // no sync until this long after the latest sync of any kind started
    };
    struct Inputs {
        bool enabled = false;      // "Auto Sync" is on
        bool configured = false;   // an account exists
        bool running = false;      // a sync is running right now
        bool editor_open = false;  // an editor page is open
        bool has_pending = false;  // at least one note has changes the server has not confirmed
        bool online = false;       // Wi-Fi is connected
        bool can_connect = false;  // offline, but the caller can ask the device to connect (once per save)
    };
    enum class Decision { Idle, Wait, Start };
    struct Step {
        Decision decision = Decision::Idle;
        int64_t wait_ms = 0;       // only for Wait: when to ask again (> 0)
        const char* reason = "";   // for the log: "", "disabled", "no_account", "nothing_pending",
                                   //   "offline", "running", "editor_open", "waiting", "connecting"
    };

    explicit SyncScheduler(Options options = {});

    void note_saved(int64_t now_ms);     // a local change was persisted: arm, remember the time
    void sync_started(int64_t now_ms);   // ANY sync started (manual, start-up, automatic)
    Step next(int64_t now_ms, const Inputs& in);
    bool armed() const;
};

}  // namespace pbnotes
```

### Behavior (each line is a test)

| Situation | `next()` result | Armed afterwards |
|-----------|-----------------|------------------|
| never saved | `Idle` | no |
| `note_saved(t)`; `next(t)` with all inputs good | `Wait(quiet_ms)` | yes |
| …`next(t + quiet_ms)` | `Start` | yes until `sync_started` |
| `sync_started(t')` with `t' >= last_save` | — | no |
| `sync_started(t')` with `t' < last_save` (save landed after the sync read the list) | — | yes |
| two saves 2 s apart: `next` after the second | `Wait` measured from the second | yes |
| `last_start` recent: `next` after the quiet period | `Wait(gap remainder)` | yes |
| the wait is the **longer** of the two remainders | — | — |
| `enabled == false` | `Idle`, reason `disabled` | no |
| `configured == false` | `Idle`, reason `no_account` | no |
| `running == true` | `Idle`, reason `running` | yes (follow-up) |
| `editor_open == true` | `Idle`, reason `editor_open` | yes |
| `has_pending == false` | `Idle`, reason `nothing_pending` | no |
| `online == false`, waits over, `can_connect == true` | `Connect`, reason `connecting` | yes (asked again when the connection is up or failed) |
| `online == false`, waits over, `can_connect == false` | `Idle`, reason `offline` | no (no retry) |
| order of checks | armed → enabled → configured → running → editor_open → has_pending → waits → online | — |
| ten saves 6 s apart over one minute, each followed by `next` at its quiet end and a start recorded | at most 5 `Start` results | — |

Waits are never ≤ 0: when the remainder is zero the result is `Start`.

## `NoteStore` (changed, `note_store.h`)

- **Settings**: `sync_after_save` added to the documented keys: `{on, off}`, default `on`;
  `set_setting("sync_after_save", v)` returns false for any other value; `get_setting` returns `off`
  when unset. `forget_account()` leaves it alone.
- **Guarded save**:

```cpp
// What the editor showed when it opened the note. The title is left out on purpose: a confirmed upload may
// rewrite it (the server's unique title) without anyone changing the text.
struct EditBase { std::string content; std::string category; bool favorite = false; };

// Like save_local, for the editor. When the stored text, category or favorite differ from `base` (a sync
// replaced the note while the editor was open) nothing is written and the result is
// EditError::ChangedOnServer ("changed_on_server"). A new ETag with the same text is an ordinary save.
EditResult save_local_if_unchanged(int64_t local_id, const EditBase& base, const std::string& title,
                                   const std::string& content, const std::string& category,
                                   bool favorite, int64_t now);
```

`EditError` gains `ChangedOnServer`. The existing `save_local` keeps its behavior and is used for every
save that did not come from an editor session (and by `saveNote` when no editor loaded the note).

- **Conflict copy of editor text**:

```cpp
// Stores `content` (the editor's text for a note a sync changed meanwhile) as a NEW pending note whose
// title carries the store's conflict suffix (" (conflict YYYY-MM-DD)", then " #2", " #3" for an unused
// title - the same rule resolve_conflict uses; the unused-title search is one shared private function).
// Staged attachments the text names are bound to the copy; downloaded ones of the original that it names
// are duplicated for it, as resolve_conflict does. Validation as create_local. The original is untouched.
EditResult save_edit_as_conflict_copy(int64_t original_local_id, const std::string& title,
                                      const std::string& content, const std::string& category,
                                      bool favorite, int64_t now);
```

No other core function changes; the sync engine, API client and transport are untouched.
