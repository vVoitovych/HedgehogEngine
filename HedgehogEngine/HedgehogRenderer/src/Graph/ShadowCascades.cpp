#include "HedgehogRenderer/Graph/ShadowCascades.hpp"

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
        }

        bool invertible = true;
        const HM::Matrix4x4 inverseCamera = (frame.View * frame.Proj).Inverse(invertible);
        const HM::Vector3   lightDir      = frame.ShadowLightDirection.value_or(HM::Vector3(1.0f, 0.0f, 0.0f));

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
            radius = std::ceil(radius * 16.0f) / 16.0f;

            const HM::Matrix4x4 lightView  = HM::Matrix4x4::LookAt(center - lightDir * radius, center,
                                                                   HM::Vector3(0.0f, 0.0f, 1.0f));
            const HM::Matrix4x4 lightOrtho = HM::Matrix4x4::Ortho(-radius, radius, -radius, radius, 0.0f, 2.0f * radius);
            cascades.ViewProj[i] = lightOrtho * lightView;

            lastSplit = splits[i];
        }
        return cascades;
    }
}
