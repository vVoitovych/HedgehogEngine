#include "HedgehogRenderer/Views/ViewCulling.hpp"

#include "HedgehogMath/api/Frustum.hpp"

#include <algorithm>

namespace Renderer
{
    namespace
    {
        constexpr uint32_t LAYER_COUNT = 32;

        bool IsOnLayer(const HX::RenderInstance& instance, uint32_t layerMask)
        {
            return instance.Layer < LAYER_COUNT && (layerMask & (1u << instance.Layer)) != 0;
        }
    }

    void CullViewInstances(std::span<const HX::RenderInstance> instances, uint32_t layerMask,
                           const HM::Matrix4x4& viewProj, ViewInstances& out,
                           std::span<const MaterialDrawInfo> materials)
    {
        out.Opaque.clear();
        out.Skinned.clear();
        out.Cutoff.clear();
        out.SkinnedCutoff.clear();
        out.Transparent.clear();
        out.SkinnedTransparent.clear();
        out.Overlay.clear();

        HM::Frustum frustum;
        frustum.ExtractPlanes(viewProj);
        for (const HX::RenderInstance& instance : instances)
        {
            if (!IsOnLayer(instance, layerMask) || !frustum.IsAABBVisible(instance.WorldBounds))
                continue;
            if (instance.Layer == HX::EDITOR_LAYER)
            {
                out.Overlay.push_back(instance);
                continue;
            }
            const bool skinned = instance.JointCount > 0;
            const auto mode    = instance.MaterialIndex < materials.size() ? materials[instance.MaterialIndex].AlphaMode
                                                                           : HedgehogEngine::MaterialAlphaMode::Opaque;
            switch (mode)
            {
            case HedgehogEngine::MaterialAlphaMode::Cutoff:
                (skinned ? out.SkinnedCutoff : out.Cutoff).push_back(instance);
                break;
            case HedgehogEngine::MaterialAlphaMode::Transparent:
                (skinned ? out.SkinnedTransparent : out.Transparent).push_back(instance);
                break;
            default:
                (skinned ? out.Skinned : out.Opaque).push_back(instance);
                break;
            }
        }
    }

    void SortBackToFront(std::span<HX::RenderInstance> instances, const HM::Vector3& eye)
    {
        const auto distance = [&eye](const HX::RenderInstance& instance)
        {
            return (instance.WorldBounds.GetCenter() - eye).LengthSqr();
        };
        std::sort(instances.begin(), instances.end(),
                  [&](const HX::RenderInstance& a, const HX::RenderInstance& b)
                  {
                      const float da = distance(a);
                      const float db = distance(b);
                      return da != db ? da > db : a.SourceId < b.SourceId;
                  });
    }

    void CollectSceneInstances(std::span<const HX::RenderInstance> instances, std::vector<HX::RenderInstance>& out)
    {
        out.clear();
        for (const HX::RenderInstance& instance : instances)
        {
            if (instance.Layer != HX::EDITOR_LAYER)
                out.push_back(instance);
        }
    }
}
