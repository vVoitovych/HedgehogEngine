#pragma once

#include "HedgehogPhysics/api/PhysicsTypes.hpp"

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Collision/Shape/Shape.h>

namespace HP
{
    // The shape with its scale baked into its size, offset by its scaled centre; a capsule turned from
    // Jolt's +Y axis to +Z, a height field from Jolt's Y-up. Null, logged, for a height field Jolt
    // cannot make. Bodies and characters share it.
    JPH::Ref<JPH::Shape> MakeShape(const ShapeDesc& shape);
}
