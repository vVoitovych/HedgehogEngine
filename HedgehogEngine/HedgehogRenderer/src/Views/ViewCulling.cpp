#include "HedgehogRenderer/Views/ViewCulling.hpp"

#include "HedgehogMath/api/Frustum.hpp"

#include <algorithm>
#include <cmath>

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

    uint32_t SelectMeshLod(const HM::AABB& worldBounds, const HM::Vector3& eye, const HM::Matrix4x4& proj,
                           uint32_t lodCount)
    {
        if (FORCE_LOD0 || lodCount <= 1)
            return 0;
        const float radius = worldBounds.GetHalfExtents().LengthSlow();
        // proj[1][1] maps view-space height onto the view's [-1, 1]; an orthographic projection has
        // w = 1 (proj[3][3]), a perspective one divides by the distance.
        float fraction = radius * std::abs(proj[1][1]);
        if (proj[3][3] == 0.0f)
        {
            const float distance = (worldBounds.GetCenter() - eye).LengthSlow();
            if (distance <= radius)
                return 0;
            fraction /= distance;
        }
        uint32_t lod       = 0;
        float    threshold = LOD1_SCREEN_FRACTION;
        while (lod + 1 < lodCount && fraction < threshold)
        {
            ++lod;
            threshold *= 0.5f;
        }
        return lod;
    }

    uint32_t SelectInstanceLod(const HX::RenderInstance& instance, const LodView& lod)
    {
        if (instance.MeshIndex >= lod.Meshes.size())
            return 0;
        return SelectMeshLod(instance.WorldBounds, lod.Eye, lod.Proj, lod.Meshes[instance.MeshIndex].LodCount);
    }

    void CullViewInstances(std::span<const HX::RenderInstance> instances, uint32_t layerMask,
                           const HM::Matrix4x4& viewProj, ViewInstances& out,
                           std::span<const MaterialDrawInfo> materials, const LodView* lod)
    {
        out.Opaque.clear();
        out.Skinned.clear();
        out.Cutoff.clear();
        out.SkinnedCutoff.clear();
        out.Transparent.clear();
        out.SkinnedTransparent.clear();
        out.Overlay.clear();
        out.Terrain.clear();

        HM::Frustum frustum;
        frustum.ExtractPlanes(viewProj);
        for (const HX::RenderInstance& visible : instances)
        {
            if (!IsOnLayer(visible, layerMask) || !frustum.IsAABBVisible(visible.WorldBounds))
                continue;
            if (visible.Layer == HX::EDITOR_LAYER)
            {
                out.Overlay.push_back(visible);
                continue;
            }
            HX::RenderInstance instance = visible;
            if (lod)
                instance.Lod = SelectInstanceLod(instance, *lod);
            if (instance.TerrainIndex >= 0)
            {
                out.Terrain.push_back(instance);
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
            if (instance.Layer != HX::EDITOR_LAYER && instance.CastsShadows)
                out.push_back(instance);
        }
    }
}
