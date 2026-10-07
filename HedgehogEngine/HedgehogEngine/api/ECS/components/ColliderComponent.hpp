#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>

namespace HedgehogEngine
{
    enum class ColliderShape
    {
        Box     = 0,
        Sphere  = 1,
        Capsule = 2, // along the entity's local +Z
    };

    // The one shape of an entity's physics body, in its local space, scaled by its transform. With
    // no RigidBodyComponent the entity is a static body. Friction and restitution live here rather
    // than on the rigid body because a static body has none. One collider per entity: Jolt keeps the
    // trigger flag, friction and restitution per body, and compound shapes are out of scope.
HH_BEGIN_COMPONENT(ColliderComponent)
    static constexpr const char* kShapeNames[] = { "Box", "Sphere", "Capsule" };
    HH_PROP_NAMED_ENUM(HedgehogEngine::ColliderShape, Shape, "Shape", HedgehogEngine::ColliderShape::Box, kShapeNames, 3)

    HH_PROP_NAMED(HM::Vector3, Center, "Center", HM::Vector3(0.0f, 0.0f, 0.0f), None)
    // A box's full extents; the default fits the default cube, which spans -1 to 1.
    HH_PROP_NAMED(HM::Vector3, Size,   "Size",   HM::Vector3(2.0f, 2.0f, 2.0f), None)
    // A sphere's or capsule's.
    HH_PROP_NAMED(float,       Radius, "Radius", 0.5f, None)
    // A capsule's length, end to end (its caps included).
    HH_PROP_NAMED(float,       Height, "Height", 2.0f, None)
    // Detects what enters it and pushes nothing.
    HH_PROP_NAMED(bool,        IsTrigger, "IsTrigger", false, None)
    // A physics layer (engine_settings.yaml's physics.layers); which pairs collide is its matrix.
    HH_PROP_NAMED_SLIDER(int32_t, Layer, "Layer", 0, 0, 15)
    HH_PROP_NAMED(float,       Friction,    "Friction",    0.5f, None)
    HH_PROP_NAMED(float,       Restitution, "Restitution", 0.0f, None)
HH_END_COMPONENT(ColliderComponent)
}
