#pragma once

#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "HedgehogPhysics/api/CharacterTypes.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>

namespace HedgehogEngine
{
    // A character the game walks (PhysicsSystem's characters, HP's CharacterVirtual): an upright
    // capsule at the entity that keeps to the ground, climbs steps, slides along walls and steep
    // slopes and pushes dynamic bodies. It needs no ColliderComponent (one beside it is ignored).
HH_BEGIN_COMPONENT(CharacterControllerComponent)
    // The capsule: its radius and its height end to end, offset by Center from the entity.
    HH_PROP_NAMED(float,       Radius,        "Radius",        0.4f, None)
    HH_PROP_NAMED(float,       Height,        "Height",        1.8f, None)
    HH_PROP_NAMED(HM::Vector3, Center,        "Center",        HM::Vector3(0.0f, 0.0f, 0.0f), None)
    // The highest step it climbs, and the steepest slope it walks up, in degrees.
    HH_PROP_NAMED(float,       StepHeight,    "StepHeight",    0.3f, None)
    HH_PROP_NAMED(float,       MaxSlopeAngle, "MaxSlopeAngle", 50.0f, None)
    HH_PROP_NAMED_SLIDER(int32_t, Layer,      "Layer",         0, 0, 15)
    HH_PROP_NAMED(float,       Mass,          "Mass",          70.0f, None)
    // The force, in newtons, it pushes dynamic bodies with.
    HH_PROP_NAMED(float,       PushStrength,  "PushStrength",  100.0f, None)
    // Times the world's gravity.
    HH_PROP_NAMED(float,       GravityScale,  "GravityScale",  1.0f, None)

    // Runtime only, never written: the character made for this entity and the entity generation it
    // was made for; the walk and jump gameplay asked for (the jump used by the next step); its state
    // after its last move.
    HP::CharacterHandle Character;
    uint32_t            Generation      = 0;
    HM::Vector3         DesiredVelocity = HM::Vector3(0.0f, 0.0f, 0.0f);
    float               PendingJump     = 0.0f;
    HP::CharacterState  State;
HH_END_COMPONENT(CharacterControllerComponent)
}
