#include "HedgehogRenderer/Graph/ShadowCascades.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{
    namespace
    {
        // Cascade tiles within the map, for each cascade count 1..4 (the legacy shadow pass's layout).
        void FillViewports(ShadowCascades& cascades, float size)
        {
            const float half = size / 2.0f;
            switch (cascades.Count)
            {
                case 1:
                    cascades.Viewports[0] = { 0.0f, 0.0f, size, size };
                    break;
                case 2:
                    cascades.Viewports[0] = { 0.0f, 0.0f, half, size };
                    cascades.Viewports[1] = { half, 0.0f, half, size };
                    break;
                case 3:
                    cascades.Viewports[0] = { 0.0f, 0.0f, half, size };
                    cascades.Viewports[1] = { half, 0.0f, half, half };
                    cascades.Viewports[2] = { half, half, half, half };
                    break;
                default:
                    cascades.Viewports[0] = { 0.0f, 0.0f, half, half };
                    cascades.Viewports[1] = { 0.0f, half, half, half };
                    cascades.Viewports[2] = { half, 0.0f, half, half };
                    cascades.Viewports[3] = { half, half, half, half };
                    break;
            }
        }

        // An orthographic projection of the box [-radius, radius]^2 in front of a look-at view,
        // from depth 0 at the eye to 1 at far, as Vulkan clips (HM::Matrix4x4::Ortho maps depth to
        // [-1, 1], so half of its box would be clipped).
        HM::Matrix4x4 MakeCascadeOrtho(float radius, float far)
        {
            HM::Matrix4x4 ortho = HM::Matrix4x4::GetIdentity();
            ortho[0][0] = 1.0f / radius;
            ortho[1][1] = -1.0f / radius; // y down, as the legacy shadow pass rendered
            ortho[2][2] = -1.0f / far;    // the view looks down -Z
            return ortho;
        }
    }

    ShadowCascades ComputeShadowCascades(const GraphFrameData& frame, uint32_t shadowMapSize)
    {
        ShadowCascades cascades;
        cascades.Count = std::clamp(frame.ShadowCascadeCount, 1u, MAX_SHADOW_CASCADES);
        FillViewports(cascades, static_cast<float>(shadowMapSize));

        const float nearClip  = frame.NearPlane;
        const float farClip   = frame.FarPlane;
        const float clipRange = farClip - nearClip;
        const float ratio     = farClip / nearClip;

        std::array<float, MAX_SHADOW_CASCADES> splits{};
        for (uint32_t i = 0; i < cascades.Count; ++i)
        {
            const float p       = static_cast<float>(i + 1) / static_cast<float>(cascades.Count);
            const float log     = nearClip * std::pow(ratio, p);
            const float uniform = nearClip + clipRange * p;
            const float d       = frame.ShadowCascadeSplitLambda * (log - uniform) + uniform;
            splits[i] = (d - nearClip) / clipRange;
            cascades.SplitDepths[i] = d;
        }

        bool invertible = true;
        const HM::Matrix4x4 inverseCamera = (frame.Proj * frame.View).Inverse(invertible);
        const HM::Vector3   lightDir      = frame.ShadowLightDirection.value_or(HM::Vector3(1.0f, 0.0f, 0.0f)).Normalize();
        // Z is up; a light straight above or below looks along it, so its view takes +Y as up.
        const HM::Vector3   lightUp       = std::abs(lightDir.z()) > 0.99f ? HM::Vector3(0.0f, 1.0f, 0.0f)
                                                                           : HM::Vector3(0.0f, 0.0f, 1.0f);

        float lastSplit = 0.0f;
        for (uint32_t i = 0; i < cascades.Count; ++i)
        {
            HM::Vector3 corners[8] =
            {
                HM::Vector3(-1.0f,  1.0f, 0.0f), HM::Vector3( 1.0f,  1.0f, 0.0f),
                HM::Vector3( 1.0f, -1.0f, 0.0f), HM::Vector3(-1.0f, -1.0f, 0.0f),
                HM::Vector3(-1.0f,  1.0f, 1.0f), HM::Vector3( 1.0f,  1.0f, 1.0f),
                HM::Vector3( 1.0f, -1.0f, 1.0f), HM::Vector3(-1.0f, -1.0f, 1.0f),
            };
            for (HM::Vector3& corner : corners)
            {
                const HM::Vector4 world = inverseCamera * HM::Vector4(corner, 1.0f);
                corner = world / world.w();
            }
            for (uint32_t j = 0; j < 4; ++j)
            {
                const HM::Vector3 edge = corners[j + 4] - corners[j];
                corners[j + 4] = corners[j] + edge * splits[i];
                corners[j]     = corners[j] + edge * lastSplit;
            }

            HM::Vector3 center(0.0f, 0.0f, 0.0f);
            for (const HM::Vector3& corner : corners)
                center += corner;
            center /= 8.0f;

            float radius = 0.0f;
            for (const HM::Vector3& corner : corners)
                radius = std::max(radius, (corner - center).Length3Slow());
            radius = std::max(std::ceil(radius * 16.0f) / 16.0f, 1.0f / 16.0f);

            // The eye sits SHADOW_CASTER_REACH radii towards the light, the far plane one radius
            // past the slice: a caster between the light and the slice still casts into it.
            const float         reach      = SHADOW_CASTER_REACH * radius;
            const HM::Matrix4x4 lightView  = HM::Matrix4x4::LookAt(center + lightDir * reach, center, lightUp);
            const HM::Matrix4x4 lightOrtho = MakeCascadeOrtho(radius, reach + radius);
            cascades.ViewProj[i]        = lightOrtho * lightView;
            cascades.WorldTexelSizes[i] = 2.0f * radius / std::max(cascades.Viewports[i].Width, 1.0f);

            lastSplit = splits[i];
        }
        return cascades;
    }

    ShadowUniform MakeShadowUniform(const ShadowCascades& cascades, const HM::Matrix4x4& cameraView,
                                    const ShadowSampling& sampling, uint32_t atlasSize, int32_t lightIndex)
    {
        ShadowUniform uniform = MakeUnshadowedUniform();
        const float   size    = static_cast<float>(std::max(atlasSize, 1u));
        uniform.CameraView    = cameraView;
        for (uint32_t i = 0; i < cascades.Count && i < MAX_SHADOW_CASCADES; ++i)
        {
            const ShadowCascadeViewport& tile = cascades.Viewports[i];
            uniform.ViewProj[i]     = cascades.ViewProj[i];
            uniform.TileRects[i][0] = tile.X / size;
            uniform.TileRects[i][1] = tile.Y / size;
            uniform.TileRects[i][2] = tile.Width / size;
            uniform.TileRects[i][3] = tile.Height / size;
            uniform.SplitDepths[i]   = cascades.SplitDepths[i];
            uniform.NormalOffsets[i] = sampling.NormalOffset * cascades.WorldTexelSizes[i];
        }
        uniform.DepthBias    = sampling.DepthBias;
        uniform.SlopeBias    = sampling.SlopeBias;
        uniform.CascadeBlend = sampling.CascadeBlend;
        uniform.TexelSize    = 1.0f / size;
        uniform.CascadeCount = static_cast<int32_t>(std::min(cascades.Count, MAX_SHADOW_CASCADES));
        uniform.PcfRadius    = static_cast<int32_t>(sampling.PcfRadius);
        uniform.LightIndex   = lightIndex;
        return uniform;
    }

    ShadowUniform MakeUnshadowedUniform()
    {
        ShadowUniform uniform;
        for (uint32_t i = 0; i < MAX_SHADOW_CASCADES; ++i)
        {
            uniform.ViewProj[i] = HM::Matrix4x4::GetIdentity();
            std::fill(std::begin(uniform.TileRects[i]), std::end(uniform.TileRects[i]), 0.0f);
            uniform.SplitDepths[i]   = 0.0f;
            uniform.NormalOffsets[i] = 0.0f;
        }
        uniform.CameraView = HM::Matrix4x4::GetIdentity();
        return uniform;
    }

    int32_t FindShadowedLight(std::span<const HX::RenderLight> lights)
    {
        for (size_t i = 0; i < lights.size(); ++i)
        {
            if (!lights[i].CastShadows)
                continue;
            const bool visible = i < static_cast<size_t>(HedgehogEngine::MAX_LIGHTS_COUNT);
            return visible && lights[i].Type == HX::LightType::Directional ? static_cast<int32_t>(i) : -1;
        }
        return -1;
    }
}
