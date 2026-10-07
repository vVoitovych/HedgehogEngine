#pragma once

#include <Jolt/Jolt.h>

#include "HedgehogMath/api/Quaternion.hpp"
#include "HedgehogMath/api/Vector.hpp"

namespace HP
{
    inline JPH::Vec3 ToJolt(const HM::Vector3& vector) { return JPH::Vec3(vector.x(), vector.y(), vector.z()); }

    inline HM::Vector3 FromJolt(JPH::Vec3Arg vector) { return HM::Vector3(vector.GetX(), vector.GetY(), vector.GetZ()); }

    // Normalized: Jolt asserts unit rotations.
    inline JPH::Quat ToJolt(const HM::Quaternion& rotation)
    {
        const HM::Quaternion unit = rotation.Normalize();
        return JPH::Quat(unit.x(), unit.y(), unit.z(), unit.w());
    }

    inline HM::Quaternion FromJolt(JPH::QuatArg rotation)
    {
        return HM::Quaternion(rotation.GetX(), rotation.GetY(), rotation.GetZ(), rotation.GetW());
    }
}
