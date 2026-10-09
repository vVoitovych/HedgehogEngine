#pragma once

#include "ContentLoaderApi.hpp"

#include <cstdint>

// IEEE 754 binary16, the texel format the environment bake hands the renderer (R16G16B16A16Float,
// R16G16Float), so it uploads without converting.
namespace ContentLoader
{
    inline constexpr float HALF_MAX = 65504.0f;

    // Rounds to the nearest half (ties to even), subnormals included; a value past HALF_MAX becomes
    // infinity, and NaN stays NaN.
    CONTENT_LOADER_API uint16_t FloatToHalf(float value);

    // Exact: every half is a float.
    CONTENT_LOADER_API float HalfToFloat(uint16_t half);
}
