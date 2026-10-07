#pragma once

#include "HedgehogPhysics/api/BodyHandle.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>

// Plain data a PhysicsWorld reports: contacts between bodies and ray hits. No Jolt type appears here.
namespace HP
{
    enum class ContactEventType : uint8_t
    {
        Enter, // two bodies began touching (or overlapping, for a sensor)
        Exit,  // they stopped, or one of them was destroyed
    };

    // A change in contact between two bodies, queued during a step and drained after it. A and B are
    // ordered by their user data, then their handles, so the same pair is always reported the same
    // way round.
    struct ContactEvent
    {
        ContactEventType Type = ContactEventType::Enter;
        BodyHandle       BodyA;
        BodyHandle       BodyB;
        uint64_t         UserDataA = 0;
        uint64_t         UserDataB = 0;
        bool             IsSensor  = false; // either body is a sensor
        // Where they touch and the direction from A towards B, for Enter; zero for Exit.
        HM::Vector3 Point  = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Normal = HM::Vector3(0.0f, 0.0f, 0.0f);
    };

    // The nearest body a ray reached.
    struct RayHit
    {
        BodyHandle  Body;
        uint64_t    UserData = 0;
        HM::Vector3 Point    = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Normal   = HM::Vector3(0.0f, 0.0f, 0.0f); // the surface's, at Point
        float       Distance = 0.0f;                          // from the ray's origin, in metres
    };

    // Every physics layer, for a query's layer mask.
    inline constexpr uint16_t ALL_LAYERS_MASK = 0xffffu;
}
