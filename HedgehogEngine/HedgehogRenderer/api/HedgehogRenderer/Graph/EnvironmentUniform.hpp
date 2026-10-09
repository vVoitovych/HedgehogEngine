#pragma once

#include "ContentLoader/api/LoadedEnvironment.hpp"

#include <cstdint>

namespace RHI
{
    class IRHITexture;
}

namespace Renderer
{
    // The scene's image-based lighting as the forward shader reads it (set 3, binding 2,
    // Common/Pbr.glsl's EnvironmentData), laid out for std140.
    struct EnvironmentUniform
    {
        // ContentLoader's SH9 irradiance (rgb; w unused), already divided by pi and times Intensity.
        float Sh[ContentLoader::SH_COEFFICIENT_COUNT][4] = {};
        // The prefiltered radiance's scale (the component's Intensity; 0 adds no specular).
        float Intensity = 0.0f;
        // The environment's turn about +Z: a world direction is turned back by it before the cube
        // and the SH are read.
        float RotationCos = 1.0f;
        float RotationSin = 0.0f;
        // The radiance cube's last mip, the one for roughness 1.
        float MaxMip = 0.0f;
    };
    static_assert(sizeof(EnvironmentUniform) == 160, "EnvironmentUniform must match Pbr.glsl's std140 block.");

    // The uniform for a baked environment: its SH and mip count, the component's intensity and
    // rotation (degrees about +Z, as SampleEquirect turns it). Non-finite values count as 0, and a
    // negative intensity as 0.
    EnvironmentUniform MakeEnvironmentUniform(const ContentLoader::ShIrradiance& sh, uint32_t mipCount, float intensity,
                                              float rotationDegrees);

    // No environment: no ambient light at all, as before image-based lighting.
    EnvironmentUniform MakeNoEnvironmentUniform();

    // What the forward pass binds for the environment (set 3, bindings 2 to 4): the uniform, the
    // prefiltered radiance cube and the split-sum BRDF table, and whether the Skybox pass draws the
    // cube (a baked map whose component shows its skybox). The textures are null in headless tests.
    struct ForwardEnvironment
    {
        EnvironmentUniform       Uniform    = MakeNoEnvironmentUniform();
        const RHI::IRHITexture*  Radiance   = nullptr;
        const RHI::IRHITexture*  BrdfLut    = nullptr;
        bool                     ShowSkybox = false;
    };
}
