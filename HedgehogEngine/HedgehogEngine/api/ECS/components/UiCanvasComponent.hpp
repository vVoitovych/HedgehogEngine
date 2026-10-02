#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>

namespace HedgehogEngine
{
    // Mirrors HUI::CanvasScaleMode, kept apart so components never include HedgehogUI.
    enum class UiCanvasScaleMode
    {
        ConstantPixelSize   = 0,
        ScaleWithTargetSize = 1
    };

    // The root of a game UI tree, drawn over the game view: its children with a UiRectComponent are
    // laid out inside the whole target, in canvas units (pixels, or reference-resolution units when
    // scaled). Canvases draw in SortOrder, lowest first, ties by entity id.
HH_BEGIN_COMPONENT(UiCanvasComponent)
    HH_PROP_NAMED(bool, IsEnabled, "Enabled", true, None)

    static constexpr const char* kScaleModeNames[] = { "Constant Pixel Size", "Scale With Target Size" };
    HH_PROP_NAMED_ENUM(HedgehogEngine::UiCanvasScaleMode, ScaleMode, "ScaleMode",
                       HedgehogEngine::UiCanvasScaleMode::ScaleWithTargetSize, kScaleModeNames, 2)

    HH_PROP_NAMED(HM::Vector2, ReferenceResolution, "ReferenceResolution", HM::Vector2(1920.0f, 1080.0f), None)
    // 0 matches the target's width, 1 its height, between them a blend in log space.
    HH_PROP_NAMED_SLIDER(float, MatchWidthOrHeight, "MatchWidthOrHeight", 0.5f, 0.0f, 1.0f)
    HH_PROP_NAMED(int32_t, SortOrder, "SortOrder", 0, None)
HH_END_COMPONENT(UiCanvasComponent)
}
