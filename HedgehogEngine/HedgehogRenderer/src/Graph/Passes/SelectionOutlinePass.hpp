#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

#include <cstdint>

namespace Renderer
{
    // How wide the selection's outline is, in pixels of the view.
    inline constexpr float SELECTION_OUTLINE_WIDTH = 2.0f;

    // The SelectionOutline shader's push constants (16 bytes, the ToneMap layout's): one texel of the
    // mask in UV, and the outline's width in pixels.
    struct SelectionOutlinePushConstants
    {
        float TexelSize[2] = { 0.0f, 0.0f };
        float Width        = SELECTION_OUTLINE_WIDTH;
        float Unused       = 0.0f;
    };
    static_assert(sizeof(SelectionOutlinePushConstants) == 16, "SelectionOutline/Outline.frag's push constant block is 16 bytes.");

    // The push constants for a mask (and colour target) of width x height pixels; zeros for an empty one.
    [[nodiscard]] SelectionOutlinePushConstants MakeSelectionOutlinePushConstants(uint32_t width, uint32_t height);

    // SelectionOutline: slots color and mask. Draws one fullscreen triangle over color (loaded), which
    // paints the selection's outline orange: every pixel outside the mask (SelectionMask's) with a
    // masked pixel within SELECTION_OUTLINE_WIDTH, its edge softened, blended over what is there.
    // Records nothing without a selection (the view's overlay instances).
    [[nodiscard]] PassTypeInfo GetSelectionOutlinePassType();
}
