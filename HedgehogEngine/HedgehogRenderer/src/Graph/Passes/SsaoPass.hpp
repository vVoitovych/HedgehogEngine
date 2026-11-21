#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"
#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <cstdint>

namespace Renderer
{
    // The terms of a projection SSAO reconstructs and projects view-space points with (Matrix4x4
    // stores columns): clip.x = A x + G, clip.y = B y + H, clip.z = C z + D, clip.w = E z + F.
    // Perspective and orthographic projections without an off-centre shear have only these.
    struct SsaoProjection
    {
        float A = 1.0f, B = 1.0f, C = 0.0f, D = 0.0f;
        float E = 0.0f, F = 1.0f, G = 0.0f, H = 0.0f;
    };

    // Ssao.pl's push constants: the projection, then radius (view units), intensity and bias, then
    // the depth's texel size.
    struct SsaoPushConstants
    {
        SsaoProjection Projection;
        float          Radius      = 0.5f;
        float          Intensity   = 1.0f;
        float          Bias        = 0.025f;
        float          Padding0    = 0.0f;
        float          TexelWidth  = 0.0f;
        float          TexelHeight = 0.0f;
        float          Padding1[2] = {};
    };
    static_assert(sizeof(SsaoPushConstants) == 64, "Ssao.frag's push constant block is 64 bytes.");

    // The blur's push constants (PostProcess.pl, 32 bytes): the depth's linearization terms (C, D,
    // E, F) and the raw occlusion's texel size.
    struct SsaoBlurPushConstants
    {
        float C = 0.0f, D = 0.0f, E = 0.0f, F = 1.0f;
        float TexelWidth  = 0.0f;
        float TexelHeight = 0.0f;
        float Padding[2]  = {};
    };
    static_assert(sizeof(SsaoBlurPushConstants) == 32, "The post-process push constant block is 32 bytes.");

    [[nodiscard]] SsaoProjection        MakeSsaoProjection(const HM::Matrix4x4& proj);
    // The settings (radius, intensity and bias at least 0; non-finite ones their defaults) and the
    // depth's texel size (0 for a zero size).
    [[nodiscard]] SsaoPushConstants     MakeSsaoPushConstants(const HM::Matrix4x4& proj, const PostProcessSettings& settings,
                                                              uint32_t depthWidth, uint32_t depthHeight);
    [[nodiscard]] SsaoBlurPushConstants MakeSsaoBlurPushConstants(const HM::Matrix4x4& proj, uint32_t rawWidth,
                                                                  uint32_t rawHeight);

    // Ssao: slots depth (sampled) and ao (written). Two passes: "<name>" draws the raw occlusion
    // into a transient of ao's size from the depth (16 hemisphere samples per pixel), "<name> blur"
    // blurs it into ao, weighting by depth so edges stay sharp. With the frame's SSAO off the first
    // records nothing and the blur only clears ao to white.
    [[nodiscard]] PassTypeInfo GetSsaoPassType();
}
