#pragma once

#include "HedgehogMath/api/Vector.hpp"

namespace HW
{
    struct InputState
    {
        bool KeyW       = false;
        bool KeyA       = false;
        bool KeyS       = false;
        bool KeyD       = false;
        bool KeyQ       = false;
        bool KeyE       = false;
        bool CtrlHeld   = false;

        bool MouseLeft   = false;
        bool MouseMiddle = false;
        bool MouseRight  = false;

        HM::Vector2 MousePos    = HM::Vector2(0.0f, 0.0f);
        HM::Vector2 MouseDelta  = HM::Vector2(0.0f, 0.0f);
        HM::Vector2 ScrollDelta = HM::Vector2(0.0f, 0.0f);
    };
}
