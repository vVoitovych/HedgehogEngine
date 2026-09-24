#pragma once

#include "View.hpp"

#include <vector>

namespace Renderer
{
    // RENDERING.md section 3.3: the order views render in is derived, never hardcoded. A view that
    // writes a target runs before every view that reads it (ViewDesc::Targets are writes,
    // ViewDesc::Reads are reads), so the editor's scene -> game -> result order, and a monitor
    // showing another monitor's feed, both fall out of the data.
    //
    // Among views with no dependency between them, lower Priority runs first, then lower ViewId.
    // Priority decides nothing else: a dependency always wins over it.
    //
    // A cycle, including a view reading its own target, is reported in `dropped` as a
    // ViewDropReason::Cycle naming every view in it, and one view leaves the cycle for this frame:
    // the lowest priority, then the newest. Ordering then repeats without it, so it always
    // terminates. Views that read a dropped view's target still render; they sample the target's
    // previous contents (RENDERING.md section 4).
    //
    // Device-free: pure logic over view metadata.
    [[nodiscard]] std::vector<View> OrderViews(std::vector<View> views, std::vector<DroppedView>& dropped);
}
