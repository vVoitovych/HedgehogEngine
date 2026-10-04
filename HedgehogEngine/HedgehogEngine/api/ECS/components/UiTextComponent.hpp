#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <string>

namespace HedgehogEngine
{
    // Mirror HUI::TextAlign and HUI::TextVerticalAlign, kept apart so components never include HedgehogUI.
    enum class UiTextAlign
    {
        Left   = 0,
        Center = 1,
        Right  = 2
    };

    enum class UiTextVerticalAlign
    {
        Top    = 0,
        Middle = 1,
        Bottom = 2
    };

    // UTF-8 text laid out in its element's rect with a TrueType font under assets:// at FontSize
    // canvas units. Saved and loaded; drawing it waits for font assets in the renderer.
HH_BEGIN_COMPONENT(UiTextComponent)
    HH_PROP_NAMED(std::string, Text,     "Text",     std::string{},                       None)
    HH_PROP_NAMED(std::string, Font,     "Font",     std::string{},                       AssetRef)
    HH_PROP_NAMED(float,       FontSize, "FontSize", 24.0f,                               None)
    HH_PROP_NAMED(HM::Vector4, Color,    "Color",    HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f), IsColor)

    static constexpr const char* kAlignNames[] = { "Left", "Center", "Right" };
    HH_PROP_NAMED_ENUM(HedgehogEngine::UiTextAlign, Align, "Align", HedgehogEngine::UiTextAlign::Left, kAlignNames, 3)
    static constexpr const char* kVerticalAlignNames[] = { "Top", "Middle", "Bottom" };
    HH_PROP_NAMED_ENUM(HedgehogEngine::UiTextVerticalAlign, VerticalAlign, "VerticalAlign",
                       HedgehogEngine::UiTextVerticalAlign::Top, kVerticalAlignNames, 3)

    HH_PROP_NAMED(bool, Wrap, "Wrap", true, None)
HH_END_COMPONENT(UiTextComponent)
}
