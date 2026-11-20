#pragma once

#include "HedgehogPhysics/api/BodyHandle.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>

// Constraints: joints holding two bodies (or a body and the world) together.
namespace HP
{
    // A constraint of a PhysicsWorld: a slot and its generation, so a handle to a destroyed constraint
    // stays invalid when its slot is reused. Plain data; invalid by default.
    struct ConstraintHandle
    {
        static constexpr uint32_t INVALID = 0xffffffffu;

        uint32_t Index      = INVALID;
        uint32_t Generation = 0;

        [[nodiscard]] constexpr bool IsSet() const { return Index != INVALID; }
        constexpr bool               operator==(const ConstraintHandle& other) const = default;
    };

    enum class ConstraintType : uint8_t
    {
        Fixed,    // holds the bodies as they are
        Hinge,    // turns them about Axis through Anchor
        Slider,   // slides them along Axis, turning neither
        Distance, // keeps Anchor on A and AnchorB on B apart by their distance (or within the limits)
    };

    // Everything in world space, as the bodies are when the constraint is made.
    struct ConstraintDesc
    {
        ConstraintType Type = ConstraintType::Fixed;
        // An unset handle is the world; at most one may be.
        BodyHandle BodyA;
        BodyHandle BodyB;
        // The joint's point on A (and, but for Distance, on B too).
        HM::Vector3 Anchor = HM::Vector3(0.0f, 0.0f, 0.0f);
        // Distance's point on B.
        HM::Vector3 AnchorB = HM::Vector3(0.0f, 0.0f, 0.0f);
        // The hinge's axis or the slider's direction; any length but zero.
        HM::Vector3 Axis = HM::Vector3(0.0f, 0.0f, 1.0f);
        // A hinge's turn from where it was made, in degrees, -180 <= min <= 0 <= max <= 180; a slider's
        // travel in metres, min <= 0 <= max; a distance in metres, 0 <= min <= max (without limits, its
        // length when made).
        bool  LimitsEnabled = false;
        float MinLimit      = 0.0f;
        float MaxLimit      = 0.0f;
        // Makes the limits soft (a distance constraint's length too): the spring's frequency in hertz
        // and its damping ratio. 0 is rigid.
        float SpringFrequency = 0.0f;
        float SpringDamping   = 0.0f;
    };
}
