#pragma once

#include <cstdint>

namespace HP
{
    // A body of a PhysicsWorld: Jolt's body id (its index and a sequence number), so a handle to a
    // destroyed body stays invalid when its index is reused. Plain data; invalid by default. A
    // handle is only meaningful to the world that made it.
    struct BodyHandle
    {
        static constexpr uint32_t INVALID = 0xffffffffu;

        uint32_t Value = INVALID;

        [[nodiscard]] constexpr bool IsSet() const { return Value != INVALID; }
        constexpr bool               operator==(const BodyHandle& other) const = default;
    };
}
