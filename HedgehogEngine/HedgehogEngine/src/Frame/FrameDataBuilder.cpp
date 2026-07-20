#include "FrameDataBuilder.hpp"

#include "HedgehogCommon/api/Camera.hpp"
#include "HedgehogCommon/api/RendererSettings.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>

namespace
{
    void AddToDrawBucket(HedgehogEngine::DrawBucket& bucket, HedgehogEngine::DrawObject object, uint64_t materialIndex)
    {
        auto it = std::find_if(bucket.begin(), bucket.end(),
            [materialIndex](const HedgehogEngine::DrawNode& node) { return node.MaterialIndex == materialIndex; });

        if (it == bucket.end())
        {
            HedgehogEngine::DrawNode node;
            node.MaterialIndex = materialIndex;
            node.Objects.push_back(object);
            bucket.push_back(std::move(node));
        }
        else
        {
            it->Objects.push_back(object);
        }
    }
}

namespace HedgehogEngine
{
    FrameData FrameDataBuilder::Build(
        const ECS::ECS&                              ecs,
        const LightSystem&                           lightSystem,
        const RenderSystem&                          renderSystem,
        const Camera&                                camera,
        float                                        deltaTime,
        const std::function<MaterialType(uint64_t)>& materialTypeLookup) const
    {
        FrameData frame;
        frame.DeltaTime = deltaTime;

        frame.Camera.View     = camera.GetViewMatrix();
        frame.Camera.Proj     = camera.GetProjectionMatrix();
        frame.Camera.Position = camera.GetPosition();
        frame.Camera.Near     = camera.GetNearPlane();
        frame.Camera.Far      = camera.GetFarPlane();

        const size_t lightCount = std::min(
            lightSystem.GetLightComponentsCount(),
            static_cast<size_t>(MAX_LIGHTS_COUNT));

        frame.Lights.reserve(lightCount);

        for (size_t i = 0; i < lightCount; ++i)
        {
            const auto& lc = lightSystem.GetLightComponentByIndex(ecs, i);
            if (!lc.Enable)
                continue;

            LightData& ld  = frame.Lights.emplace_back();
            ld.Position  = lc.Position;
            ld.Direction = lc.Direction;
            ld.Color     = lc.Color;
            ld.Intensity = lc.Intensity;
            ld.Radius    = lc.Radius;
            ld.ConeAngle = lc.ConeAngle;
            ld.Type      = static_cast<int>(lc.LightType);
        }

        frame.ShadowLightDirection = lightSystem.GetShadowDir();

        BuildDrawList(ecs, renderSystem, materialTypeLookup, frame.DrawList);

        return frame;
    }

    void FrameDataBuilder::BuildDrawList(
        const ECS::ECS&                              ecs,
        const RenderSystem&                          renderSystem,
        const std::function<MaterialType(uint64_t)>& materialTypeLookup,
        DrawList&                                    outDrawList) const
    {
        for (auto entity : renderSystem.GetEntities())
        {
            const auto& renderComponent = ecs.GetComponent<RenderComponent>(entity);
            if (!renderComponent.IsVisible || !renderComponent.MaterialIndex.has_value())
                continue;

            const auto& meshComponent = ecs.GetComponent<MeshComponent>(entity);
            if (!meshComponent.MeshIndex.has_value())
            {
                LOGERROR("Entity ", entity, " has a render component but no mesh index.");
                continue;
            }

            const uint64_t materialIndex = renderComponent.MaterialIndex.value();
            const uint64_t meshIndex     = meshComponent.MeshIndex.value();
            const auto&    transform     = ecs.GetComponent<TransformComponent>(entity);

            DrawObject object;
            object.MeshIndex = meshIndex;
            object.Transform = transform.ObjMatrix;

            const MaterialType matType = materialTypeLookup(materialIndex);

            switch (matType)
            {
            case MaterialType::Opaque:
                AddToDrawBucket(outDrawList.Opaque, object, materialIndex);
                break;
            case MaterialType::Cutoff:
                AddToDrawBucket(outDrawList.Cutoff, object, materialIndex);
                break;
            case MaterialType::Transparent:
                AddToDrawBucket(outDrawList.Transparent, object, materialIndex);
                break;
            default:
                LOGERROR("Unknown material type for material index ", materialIndex);
                break;
            }
        }
    }
}
