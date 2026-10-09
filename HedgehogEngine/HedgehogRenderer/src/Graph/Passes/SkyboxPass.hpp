#pragma once

#include "HedgehogRenderer/Graph/PassBuilderRegistry.hpp"

#include "HedgehogMath/api/Matrix.hpp"

namespace Renderer
{
    struct EnvironmentUniform;

    // The Skybox pipeline's fragment push constants: the inverse of the view-projection without the
    // camera's translation (clip space to a world direction), then the environment's turn about +Z
    // and its intensity.
    struct SkyboxPushConstants
    {
        float InverseViewProj[16] = {};
        float RotationCos         = 1.0f;
        float RotationSin         = 0.0f;
        float Intensity           = 0.0f;
        float Padding             = 0.0f;
    };
    static_assert(sizeof(SkyboxPushConstants) == 80, "The Skybox fragment shader's push constant block is 80 bytes.");

    // The constants for a camera and an environment: the view's translation removed, so the sky
    // stays at infinity whichever way the camera moves.
    [[nodiscard]] SkyboxPushConstants MakeSkyboxPushConstants(const HM::Matrix4x4& view, const HM::Matrix4x4& proj,
                                                              const EnvironmentUniform& environment);

    // Skybox: slots color (written over, loaded) and depth (read-only). Draws the environment's
    // radiance cube at mip 0 where nothing else is: one fullscreen triangle at the far plane, depth
    // tested less-or-equal against the view's depth, which it does not write. Records nothing when
    // the frame's environment does not show a skybox (GraphFrameData::Environment.ShowSkybox).
    [[nodiscard]] PassTypeInfo GetSkyboxPassType();
}
