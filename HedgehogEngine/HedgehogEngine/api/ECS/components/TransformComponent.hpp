#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogMath/api/Matrix.hpp"

namespace HedgehogEngine
{
HH_BEGIN_COMPONENT(TransformComponent)
    HH_PROP_NAMED(HM::Vector3, m_Position, "Position", HM::Vector3(0.0f, 0.0f, 0.0f), None)
    HH_PROP_NAMED(HM::Vector3, m_Rotation, "Rotation", HM::Vector3(0.0f, 0.0f, 0.0f), None)
    HH_PROP_NAMED(HM::Vector3, m_Scale,    "Scale",    HM::Vector3(1.0f, 1.0f, 1.0f), None)

    HM::Matrix4x4 m_LocalMatrix = HM::Matrix4x4(); // local-space T·Rx·Ry·Rz·S, not serialised
    HM::Matrix4x4 m_ObjMatrix   = HM::Matrix4x4(); // world-space parent·local, not serialised
HH_END_COMPONENT(TransformComponent)
}
