#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

#include "HedgehogExtract/api/UiDrawList.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include "RHI/api/RHITypes.hpp"

#include <cstdint>

namespace Renderer
{
    // The GameUi pipeline's push constants: clip position = pixel position * Scale + Offset.
    struct GameUiPushConstants
    {
        float Scale[2]  = { 1.0f, 1.0f };
        float Offset[2] = { 0.0f, 0.0f };
    };
    static_assert(sizeof(GameUiPushConstants) == 16, "The GameUi vertex shader's push constant block is 16 bytes.");

    // Maps a uiTargetSize-pixel UI, (0, 0) at its top-left and y down, onto the whole viewport.
    [[nodiscard]] GameUiPushConstants MakeGameUiPushConstants(const HM::Vector2& uiTargetSize);

    // A draw command's scissor (pixels of the UI's target) in pixels of a width x height colour
    // target, rounded outward and clamped to it; zero-sized when nothing of it is inside.
    [[nodiscard]] RHI::Scissor MakeGameUiScissor(const HX::UiRect& scissor, const HM::Vector2& uiTargetSize,
                                                 uint32_t width, uint32_t height);

    // GameUi: slot color. Draws the frame's game UI (GraphFrameData::UiCommands) over what the view
    // has drawn, alpha-blended and without depth: one indexed draw per command, with its scissor and
    // texture. Records nothing when the frame has no UI to draw.
    [[nodiscard]] PassTypeInfo GetGameUiPassType();
}
