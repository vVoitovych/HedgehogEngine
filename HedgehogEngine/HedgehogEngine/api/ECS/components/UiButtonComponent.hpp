#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"

namespace HedgehogEngine
{
    enum class UiButtonState
    {
        Normal  = 0,
        Hovered = 1,
        Pressed = 2
    };

    // Makes its element a button: the element's image is tinted (multiplied) by the tint of the
    // button's state, or by DisabledTint while it is not interactable.
HH_BEGIN_COMPONENT(UiButtonComponent)
    HH_PROP_NAMED(bool,        IsInteractable, "Interactable", true,                                None)
    HH_PROP_NAMED(HM::Vector4, NormalTint,     "NormalTint",   HM::Vector4(1.0f, 1.0f, 1.0f, 1.0f), IsColor)
    HH_PROP_NAMED(HM::Vector4, HoverTint,      "HoverTint",    HM::Vector4(0.9f, 0.9f, 0.9f, 1.0f), IsColor)
    HH_PROP_NAMED(HM::Vector4, PressedTint,    "PressedTint",  HM::Vector4(0.7f, 0.7f, 0.7f, 1.0f), IsColor)
    HH_PROP_NAMED(HM::Vector4, DisabledTint,   "DisabledTint", HM::Vector4(0.5f, 0.5f, 0.5f, 0.5f), IsColor)

    UiButtonState State = UiButtonState::Normal; // runtime: set by input handling
HH_END_COMPONENT(UiButtonComponent)
}
