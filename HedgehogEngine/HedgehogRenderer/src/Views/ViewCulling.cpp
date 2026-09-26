#include "HedgehogRenderer/Views/ViewCulling.hpp"

#include "HedgehogMath/api/Frustum.hpp"

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
                           const HM::Matrix4x4& viewProj, ViewInstances& out)
    {
        out.Opaque.clear();
        out.Overlay.clear();

        HM::Frustum frustum;
        frustum.ExtractPlanes(viewProj);
        for (const HX::RenderInstance& instance : instances)
        {
            if (!IsOnLayer(instance, layerMask) || !frustum.IsAABBVisible(instance.WorldBounds))
                continue;
            if (instance.Layer == HX::EDITOR_LAYER)
                out.Overlay.push_back(instance);
            else
                out.Opaque.push_back(instance);
        }
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
