#pragma once

#include "HedgehogMath/api/Vector.hpp"

namespace HW
{
    struct InputState
    {
        bool m_KeyW       = false;
        bool m_KeyA       = false;
        bool m_KeyS       = false;
        bool m_KeyD       = false;
        bool m_KeyQ       = false;
        bool m_KeyE       = false;
        bool m_CtrlHeld   = false;

        bool m_MouseLeft   = false;
        bool m_MouseMiddle = false;
        bool m_MouseRight  = false;

        HM::Vector2 m_MousePos    = HM::Vector2(0.0f, 0.0f);
        HM::Vector2 m_MouseDelta  = HM::Vector2(0.0f, 0.0f);
        HM::Vector2 m_ScrollDelta = HM::Vector2(0.0f, 0.0f);
    };
}
