#pragma once

#include "HedgehogMath/api/Vector.hpp"

namespace Context
{
    struct Light
    {
        alignas(16) HM::Vector3 m_Position;
        alignas(16) HM::Vector3 m_Direction;
        alignas(16) HM::Vector3 m_Color;
        alignas(16) HM::Vector4 m_Data;  // (type, intensity, radius, cos(coneAngle))
    };
}
