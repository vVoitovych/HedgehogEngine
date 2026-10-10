#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

namespace Renderer
{
    // SelectionMask: slot mask. Clears the mask (an R8Unorm target) and draws the view's overlay
    // instances (the editor's selection, on the editor layer) into it as solid white, with no depth
    // test and no face culling, so the whole silhouette is marked even where the scene hides it. The
    // SelectionOutline pass reads it. Records nothing without overlay instances.
    [[nodiscard]] PassTypeInfo GetSelectionMaskPassType();
}
