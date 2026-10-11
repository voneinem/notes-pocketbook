#pragma once
// Page step of the reader that never cuts a table row or an image (spec 002 FR-012, research R-11).
// Pure arithmetic, so it is tested on the host; the QML only asks for the result.

#include <vector>

namespace pbnotes {

struct Interval {  // an unsplittable block in content coordinates
    double top, bottom;
};

// Scroll offset of the next page. `step` is the plain page distance, `direction` is +1 (down) or -1 (up).
// If an unsplittable block straddles the new top edge, the edge moves to that block's top, so it is shown
// whole (going down the next page starts at the block; going up the new top moves up to it). Blocks taller
// than the view are ignored, and a move that would not make progress falls back to the plain step. The
// result is clamped to [0, content_height - view_height] (0 when the content is shorter than the view).
double page_target(double content_y, double view_height, double content_height, double step, int direction,
                   const std::vector<Interval>& unsplittable);

}  // namespace pbnotes
