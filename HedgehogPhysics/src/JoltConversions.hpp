#pragma once

#include <Jolt/Jolt.h>

#include "HedgehogMath/api/Vector.hpp"

namespace HP
{
    inline JPH::Vec3 ToJolt(const HM::Vector3& vector) { return JPH::Vec3(vector.x(), vector.y(), vector.z()); }

    inline HM::Vector3 FromJolt(JPH::Vec3Arg vector) { return HM::Vector3(vector.GetX(), vector.GetY(), vector.GetZ()); }
}
