#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"

namespace HedgehogEngine
{
    // A UI element's rect inside its parent's (HUI::RectTransform's fields): anchors as fractions of
    // the parent, (0, 0) its top-left and y down; the pivot; an offset; and a size added to the
    // anchors' span. An element hidden with IsVisible draws nothing, and neither do its children.
HH_BEGIN_COMPONENT(UiRectComponent)
    HH_PROP_NAMED(bool,        IsVisible, "Visible",   true,                    None)
    HH_PROP_NAMED(HM::Vector2, AnchorMin, "AnchorMin", HM::Vector2(0.5f, 0.5f), None)
    HH_PROP_NAMED(HM::Vector2, AnchorMax, "AnchorMax", HM::Vector2(0.5f, 0.5f), None)
    HH_PROP_NAMED(HM::Vector2, Pivot,     "Pivot",     HM::Vector2(0.5f, 0.5f), None)
    HH_PROP_NAMED(HM::Vector2, Offset,    "Offset",    HM::Vector2(0.0f, 0.0f), None)
    HH_PROP_NAMED(HM::Vector2, Size,      "Size",      HM::Vector2(100.0f, 100.0f), None)
HH_END_COMPONENT(UiRectComponent)
}
