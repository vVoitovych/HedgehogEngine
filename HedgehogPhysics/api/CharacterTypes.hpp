#pragma once

#include "HedgehogPhysics/api/PhysicsTypes.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>

// Characters: capsules moved by the game rather than simulated (Jolt's CharacterVirtual), which walk
// on what they stand on, slide along walls, climb steps, keep to the floor going down slopes and push
// dynamic bodies. All Z-up, as the rest of the world.
namespace HP
{
    // A character of a PhysicsWorld: a slot and its generation, so a handle to a destroyed character
    // stays invalid when its slot is reused. Plain data; invalid by default.
    struct CharacterHandle
    {
        static constexpr uint32_t INVALID = 0xffffffffu;

        uint32_t Index      = INVALID;
        uint32_t Generation = 0;

        [[nodiscard]] constexpr bool IsSet() const { return Index != INVALID; }
        constexpr bool               operator==(const CharacterHandle& other) const = default;
    };

    struct CharacterDesc
    {
        // A capsule along +Z: its radius and its cylinder's half height (0 a sphere), offset by Center
        // from the character's position.
        float       Radius     = 0.3f;
        float       HalfHeight = 0.6f;
        HM::Vector3 Center     = HM::Vector3(0.0f, 0.0f, 0.0f);
        BodyPose    Pose;
        uint32_t    Layer = 0; // a physics layer, below PHYSICS_LAYER_COUNT
        float       Mass  = 70.0f;
        // The force, in newtons, it pushes dynamic bodies with.
        float MaxStrength = 100.0f;
        // The steepest ground it can walk up; steeper is a wall it slides down.
        float MaxSlopeDegrees = 50.0f;
        // The highest step it climbs while walking, and how far it is pulled down onto the floor
        // when walking down slopes and steps (0 turns either off).
        float StepHeight           = 0.3f;
        float StickToFloorDistance = 0.5f;
        // How far ahead of the capsule contacts are found.
        float    PredictiveContactDistance = 0.1f;
        uint64_t UserData                  = 0; // also its inner body's, so rays and contacts name it
    };

    enum class CharacterGroundState
    {
        OnGround,      // on ground it can walk on
        OnSteepGround, // on ground too steep to walk up
        NotSupported,  // touching something that does not hold it up
        InAir,
    };

    struct CharacterState
    {
        HM::Vector3          Position       = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3          LinearVelocity = HM::Vector3(0.0f, 0.0f, 0.0f);
        CharacterGroundState Ground         = CharacterGroundState::InAir;
        // Of what it stands on (zero and 0 when in the air): the surface's normal, its velocity at the
        // contact (a moving platform's) and its body's user data.
        HM::Vector3 GroundNormal   = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 GroundVelocity = HM::Vector3(0.0f, 0.0f, 0.0f);
        uint64_t    GroundUserData = 0;
    };
}
