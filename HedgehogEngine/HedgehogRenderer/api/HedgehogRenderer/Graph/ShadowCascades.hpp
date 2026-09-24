#pragma once

#include "GraphFrameContext.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <array>
#include <cstdint>

namespace Renderer
{
    inline constexpr uint32_t MAX_SHADOW_CASCADES = 4;

    struct ShadowCascadeViewport
    {
        float X      = 0.0f;
        float Y      = 0.0f;
        float Width  = 0.0f;
        float Height = 0.0f;
    };

    // Each cascade's light view-projection and the region of the shadow map it renders into.
    struct ShadowCascades
    {
        uint32_t                                                Count = 0;
        std::array<HM::Matrix4x4, MAX_SHADOW_CASCADES>          ViewProj;
        std::array<ShadowCascadeViewport, MAX_SHADOW_CASCADES>  Viewports;
    };

    // Splits the view frustum into frame.ShadowCascadeCount cascades (clamped to 1..4) with the
    // practical split scheme, fits a light-space ortho box around each, and tiles them into a
    // shadowMapSize-square map. Pure: the same math as the legacy ShadowmapPass, so both paths
    // produce the same cascades.
    [[nodiscard]] ShadowCascades ComputeShadowCascades(const GraphFrameData& frame, uint32_t shadowMapSize);
}
