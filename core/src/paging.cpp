#include "pbnotes/paging.h"

#include <algorithm>

namespace pbnotes {

double page_target(double content_y, double view_height, double content_height, double step, int direction,
                   const std::vector<Interval>& unsplittable) {
    const double max_y = std::max(0.0, content_height - view_height);
    const double dir = direction < 0 ? -1.0 : 1.0;
    double target = content_y + dir * step;
    for (const Interval& b : unsplittable) {
        if (b.bottom - b.top > view_height) continue;  // never fits: ignore, so paging still progresses
        if (b.top < target && target < b.bottom) {
            // forward only counts if the block's top is ahead of where we are (otherwise: plain step)
            if (dir > 0 && b.top > content_y) target = b.top;
            else if (dir < 0) target = b.top;
            break;
        }
    }
    return std::min(max_y, std::max(0.0, target));
}

}  // namespace pbnotes
