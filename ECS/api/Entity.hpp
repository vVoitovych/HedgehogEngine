#pragma once

#include <bitset>
#include <cstddef>
#include <limits>

namespace ECS
{
    // Every registered component type keeps a fixed array of MAX_ENTITIES components, and every
    // entity a MAX_COMPONENTS-bit signature: raising either costs memory (see PERFORMANCE.md).
    inline constexpr size_t MAX_ENTITIES   = 4096;
    inline constexpr size_t MAX_COMPONENTS = 64;

    using Entity        = size_t;
    using Signature     = std::bitset<MAX_COMPONENTS>;
    using ComponentType = size_t;

    inline constexpr Entity INVALID_ENTITY = std::numeric_limits<Entity>::max();
}
