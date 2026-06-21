#pragma once

#include <cstdint>

namespace Reflection
{
    enum class PropertyFlags : uint32_t
    {
        None     = 0,
        Hidden   = 1 << 0,
        IsSlider = 1 << 1,
        IsColor  = 1 << 2,
    };

    constexpr PropertyFlags operator|(PropertyFlags a, PropertyFlags b)
    {
        return static_cast<PropertyFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
    }

    constexpr bool HasFlag(PropertyFlags flags, PropertyFlags test)
    {
        return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(test)) != 0;
    }
}
