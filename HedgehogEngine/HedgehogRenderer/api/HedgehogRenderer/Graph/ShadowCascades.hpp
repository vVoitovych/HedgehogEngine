#pragma once

#include "GraphFrameContext.hpp"

#include "HedgehogMath/api/Matrix.hpp"

#include <array>
#include <cstdint>
#include <span>

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

    // Each cascade's light view-projection, the region of the shadow map it renders into, the view
    // depth its slice of the camera frustum ends at and the world size of one of its texels.
    struct ShadowCascades
    {
        uint32_t                                                Count = 0;
        std::array<HM::Matrix4x4, MAX_SHADOW_CASCADES>          ViewProj;
        std::array<ShadowCascadeViewport, MAX_SHADOW_CASCADES>  Viewports;
        std::array<float, MAX_SHADOW_CASCADES>                  SplitDepths{};    // distance in front of the camera
        std::array<float, MAX_SHADOW_CASCADES>                  WorldTexelSizes{};
    };

    // Splits the view frustum into frame.ShadowCascadeCount cascades (clamped to 1..4) with the
    // practical split scheme, fits a light-space ortho box around each (depth 0 to 1, as Vulkan
    // clips, reaching SHADOW_CASTER_REACH box radii back towards the light so casters in front of
    // the slice are kept), and tiles them into a shadowMapSize-square map. Pure.
    [[nodiscard]] ShadowCascades ComputeShadowCascades(const GraphFrameData& frame, uint32_t shadowMapSize);

    // How far (in cascade radii) a cascade's depth range reaches from its slice back towards the light.
    inline constexpr float SHADOW_CASTER_REACH = 4.0f;

    // How the forward pass samples the atlas (HedgehogSettings::ShadowmapSettings' sampling values).
    struct ShadowSampling
    {
        float    DepthBias    = 0.0005f;
        float    SlopeBias    = 0.002f;
        float    NormalOffset = 1.0f; // texels of the receiver's cascade
        uint32_t PcfRadius    = 1;
        float    CascadeBlend = 0.1f;
    };

    // The forward pass's set 3, binding 0 (Common/Shadows.glsl), std140: what it needs to read the
    // sun's shadow from the atlas at binding 1. CascadeCount 0 (or LightIndex -1) means unshadowed.
    struct ShadowUniform
    {
        HM::Matrix4x4 ViewProj[MAX_SHADOW_CASCADES]; // offset 0: world to each cascade's clip space
        HM::Matrix4x4 CameraView;                     // offset 256: the shadow view's camera, for the depth
        float         TileRects[MAX_SHADOW_CASCADES][4]; // offset 320: each tile's x, y, width, height in atlas UV
        float         SplitDepths[MAX_SHADOW_CASCADES];  // offset 384: where each cascade ends, in front of the camera
        float         NormalOffsets[MAX_SHADOW_CASCADES]; // offset 400: the normal offset in world units per cascade
        float         DepthBias    = 0.0f;            // offset 416
        float         SlopeBias    = 0.0f;            // offset 420
        float         CascadeBlend = 0.0f;            // offset 424
        float         TexelSize    = 0.0f;            // offset 428: 1 / atlas size
        int32_t       CascadeCount = 0;               // offset 432
        int32_t       PcfRadius    = 0;               // offset 436
        int32_t       LightIndex   = -1;              // offset 440: the shadowed light in SceneLightsUniform
        int32_t       Padding      = 0;               // to 448
    };
    static_assert(sizeof(ShadowUniform) == 448, "ShadowUniform must match Common/Shadows.glsl's std140 block");

    // Packs the frame's cascades, the shadow view's camera, the sampling values and the shadowed
    // light (its index among the scene lights, or -1) for the forward pass.
    [[nodiscard]] ShadowUniform MakeShadowUniform(const ShadowCascades& cascades, const HM::Matrix4x4& cameraView,
                                                  const ShadowSampling& sampling, uint32_t atlasSize,
                                                  int32_t lightIndex);

    // A uniform that shadows nothing: bound when the frame has no shadow view.
    [[nodiscard]] ShadowUniform MakeUnshadowedUniform();

    // The light the sun's shadow falls from: the first that casts shadows, when it is directional
    // and within MAX_LIGHTS_COUNT (the scene lights the shader sees); else -1.
    [[nodiscard]] int32_t FindShadowedLight(std::span<const HX::RenderLight> lights);
}
