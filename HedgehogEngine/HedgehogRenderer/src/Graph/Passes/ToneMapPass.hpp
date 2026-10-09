#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

namespace Renderer
{
    // The ToneMap pipeline's push constants: the exposure as a linear scale of the radiance.
    struct ToneMapPushConstants
    {
        float ExposureScale = 1.0f;
        float Padding[3]    = { 0.0f, 0.0f, 0.0f };
    };
    static_assert(sizeof(ToneMapPushConstants) == 16, "The ToneMap fragment shader's push constant block is 16 bytes.");

    // 2^exposureEv: +1 EV doubles the radiance before the curve. A non-finite exposure is 0 EV.
    [[nodiscard]] ToneMapPushConstants MakeToneMapPushConstants(float exposureEv);

    // ToneMap: slots hdr (sampled) and color (written). Maps the view's HDR radiance onto its LDR
    // colour: one fullscreen triangle scaling hdr by the frame's exposure (GraphFrameData::Exposure)
    // and applying the ACES filmic curve, the result linear. Every pixel of color is written.
    [[nodiscard]] PassTypeInfo GetToneMapPassType();
}
