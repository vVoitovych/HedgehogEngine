#pragma once

#include <bitset>
#include <cstddef>

namespace ECS
{
    inline constexpr size_t MAX_ENTITIES   = 512;
    inline constexpr size_t MAX_COMPONENTS = 16;

    using Entity        = size_t;
    using Signature     = std::bitset<MAX_COMPONENTS>;
    using ComponentType = size_t;
}
