#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogPhysics/api/ConstraintTypes.hpp"

#include "ECS/api/Entity.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>

namespace HedgehogEngine
{
    // As HP::ConstraintType, by value.
    enum class JointType
    {
        Fixed    = 0,
        Hinge    = 1,
        Slider   = 2,
        Distance = 3,
    };

    // Joins the entity's physics body to ConnectedEntity's (none: the world) in Play (PhysicsSystem's
    // joints, HP's constraints). Anchor and Axis are in the entity's local space, taken into the world
    // as the bodies are when the joint is made; a Distance joint keeps Anchor and ConnectedAnchor (in
    // the connected entity's local space, or world space for the world) apart by their distance then,
    // or within its limits.
HH_BEGIN_COMPONENT(JointComponent)
    HH_PROP_NAMED(bool, Enabled, "Enabled", true, None)
    static constexpr const char* kTypeNames[] = { "Fixed", "Hinge", "Slider", "Distance" };
    HH_PROP_NAMED_ENUM(HedgehogEngine::JointType, Type, "Type", HedgehogEngine::JointType::Hinge, kTypeNames, 4)
    HH_PROP_NAMED(ECS::Entity, ConnectedEntity, "ConnectedEntity", ECS::INVALID_ENTITY, EntityRef)
    HH_PROP_NAMED(HM::Vector3, Anchor, "Anchor", HM::Vector3(0.0f, 0.0f, 0.0f), None)
    // A hinge's axis or a slider's direction.
    HH_PROP_NAMED(HM::Vector3, Axis,   "Axis",   HM::Vector3(0.0f, 0.0f, 1.0f), None)
    // A Distance joint's other end.
    HH_PROP_NAMED(HM::Vector3, ConnectedAnchor, "ConnectedAnchor", HM::Vector3(0.0f, 0.0f, 0.0f), None)
    // Degrees for a hinge (-180 <= min <= 0 <= max <= 180), metres for a slider (min <= 0 <= max) or a
    // distance (0 <= min <= max).
    HH_PROP_NAMED(bool,  LimitsEnabled,   "LimitsEnabled",   false, None)
    HH_PROP_NAMED(float, MinLimit,        "MinLimit",        0.0f, None)
    HH_PROP_NAMED(float, MaxLimit,        "MaxLimit",        0.0f, None)
    // Makes the limits (a distance's length too) a spring: hertz and a damping ratio; 0 is rigid.
    HH_PROP_NAMED(float, SpringFrequency, "SpringFrequency", 0.0f, None)
    HH_PROP_NAMED(float, SpringDamping,   "SpringDamping",   0.0f, None)

    // Runtime only, never written: the constraint made for this entity.
    HP::ConstraintHandle Constraint;
HH_END_COMPONENT(JointComponent)
}
