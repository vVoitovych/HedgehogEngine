#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"
#include "HedgehogMath/api/Matrix.hpp"

namespace HedgehogEngine
{
HH_BEGIN_COMPONENT(TransformComponent)
    HH_PROP_NAMED(HM::Vector3, Position, "Position", HM::Vector3(0.0f, 0.0f, 0.0f), None)
    HH_PROP_NAMED(HM::Vector3, Rotation, "Rotation", HM::Vector3(0.0f, 0.0f, 0.0f), None)
    HH_PROP_NAMED(HM::Vector3, Scale,    "Scale",    HM::Vector3(1.0f, 1.0f, 1.0f), None)

    HM::Matrix4x4 LocalMatrix = HM::Matrix4x4(); // local-space T·Rx·Ry·Rz·S, not serialised
    HM::Matrix4x4 ObjMatrix   = HM::Matrix4x4(); // world-space parent·local, not serialised
HH_END_COMPONENT(TransformComponent)
}
