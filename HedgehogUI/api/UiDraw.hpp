#pragma once

#include "HedgehogExtract/api/UiDrawList.hpp"

#include <cstdint>

// Writing quads into a HX::UiDrawList, as free functions over the caller's list.
namespace HUI
{
    // One rectangle to draw, in the target's pixels.
    struct UiQuad
    {
        HX::UiRect Rect;
        uint32_t   Color   = 0xFFFFFFFF;                   // PackColor's RGBA8
        HX::UiRect Uv      = { 0.0f, 0.0f, 1.0f, 1.0f };   // the texture's sub-rect, in [0, 1]
        uint32_t   Texture = HX::UI_NO_TEXTURE;
    };

    // RGBA8 with red in the lowest byte, each channel clamped to [0, 1] and rounded.
    [[nodiscard]] uint32_t PackColor(float r, float g, float b, float a = 1.0f);

    // Appends quad as two triangles clipped to scissor (a pixel rect). It joins the last command when
    // that command has the same texture and scissor and room for four more 16-bit-indexed vertices;
    // otherwise it starts a new one. A quad with no area, or entirely outside scissor, adds nothing.
    void AppendQuad(HX::UiDrawList& list, const UiQuad& quad, const HX::UiRect& scissor);
}
