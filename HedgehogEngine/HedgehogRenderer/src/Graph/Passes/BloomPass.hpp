#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

#include <cstdint>

namespace Renderer
{
    // The bloom chain's depth: levels at 1/2 to 1/32 of the view.
    inline constexpr uint32_t BLOOM_LEVELS = 5;

    // BloomDownsample's push constants (PostProcess.pl, 32 bytes): the source's texel size, and for
    // the first level (Prefilter 1) the soft-knee threshold applied to the HDR radiance.
    struct BloomDownsamplePushConstants
    {
        float    TexelWidth  = 0.0f;
        float    TexelHeight = 0.0f;
        float    Threshold   = 1.0f;
        float    Knee        = 0.5f; // the threshold times BloomKnee
        uint32_t Prefilter   = 0;
        float    Padding[3]  = {};
    };
    static_assert(sizeof(BloomDownsamplePushConstants) == 32, "The post-process push constant block is 32 bytes.");

    // BloomUpsample's push constants: the source's texel size (the tent spans one texel of it).
    struct BloomUpsamplePushConstants
    {
        float TexelWidth  = 0.0f;
        float TexelHeight = 0.0f;
        float Padding[6]  = {};
    };
    static_assert(sizeof(BloomUpsamplePushConstants) == 32, "The post-process push constant block is 32 bytes.");

    // A threshold and knee that are not finite and at least 0 take the defaults'.
    [[nodiscard]] BloomDownsamplePushConstants MakeBloomDownsamplePushConstants(uint32_t sourceWidth, uint32_t sourceHeight,
                                                                                const PostProcessSettings& settings,
                                                                                bool prefilter);
    [[nodiscard]] BloomUpsamplePushConstants MakeBloomUpsamplePushConstants(uint32_t sourceWidth, uint32_t sourceHeight);

    // Bloom: slots hdr (sampled) and bloom (written, half the view). "<name> down 0" to "down 4"
    // halve the HDR radiance level by level (the first thresholded, a Karis average against
    // fireflies), each into a transient of its own; "<name> up 3" to "up 0" add each level, tent
    // filtered, onto the one above, and "<name>" draws level 0 into bloom. With the frame's bloom off
    // only bloom is cleared to black.
    [[nodiscard]] PassTypeInfo GetBloomPassType();
}
