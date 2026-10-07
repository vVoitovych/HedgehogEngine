#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogPhysics/api/BodyHandle.hpp"

#include <cstdint>

namespace HedgehogEngine
{
    enum class RigidBodyType
    {
        Static    = 0, // never moves
        Kinematic = 1, // follows its transform, pushes dynamic bodies, is not pushed
        Dynamic   = 2, // simulated: gravity, forces and contacts move it
    };

    // How an entity's body moves; its shape is its ColliderComponent, without which it has no body.
HH_BEGIN_COMPONENT(RigidBodyComponent)
    static constexpr const char* kBodyTypeNames[] = { "Static", "Kinematic", "Dynamic" };
    HH_PROP_NAMED_ENUM(HedgehogEngine::RigidBodyType, BodyType, "BodyType", HedgehogEngine::RigidBodyType::Dynamic,
                       kBodyTypeNames, 3)

    // Kilograms, a dynamic body's.
    HH_PROP_NAMED(float, Mass,           "Mass",           1.0f,  None)
    HH_PROP_NAMED(float, LinearDamping,  "LinearDamping",  0.05f, None)
    HH_PROP_NAMED(float, AngularDamping, "AngularDamping", 0.05f, None)
    // Times the world's gravity.
    HH_PROP_NAMED(float, GravityScale,   "GravityScale",   1.0f,  None)

    // Runtime only, never written: the body made for this entity and the entity generation it was
    // made for, so a recycled entity id never reaches a stale body.
    HP::BodyHandle Body;
    uint32_t       BodyGeneration = 0;
HH_END_COMPONENT(RigidBodyComponent)
}
