#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

#include <cstdint>

namespace Renderer
{
    // The post-process pipelines' push constants (PostProcess.pl, 32 bytes), as the Fxaa pass fills
    // them: the source's texel size and whether to filter (1) or pass the pixel through (0).
    struct FxaaPushConstants
    {
        float    TexelWidth  = 0.0f;
        float    TexelHeight = 0.0f;
        uint32_t Enabled     = 1;
        float    Padding[5]  = {};
    };
    static_assert(sizeof(FxaaPushConstants) == 32, "The post-process push constant block is 32 bytes.");

    // The texel size of a width x height source (0 for a zero size) and the mode.
    [[nodiscard]] FxaaPushConstants MakeFxaaPushConstants(uint32_t width, uint32_t height, bool enabled);

    // Fxaa: slots ldr (sampled) and color (written whole). One fullscreen triangle smoothing the
    // tone-mapped image's edges, or copying it when the frame's post-processing turns FXAA off.
    [[nodiscard]] PassTypeInfo GetFxaaPassType();
}
