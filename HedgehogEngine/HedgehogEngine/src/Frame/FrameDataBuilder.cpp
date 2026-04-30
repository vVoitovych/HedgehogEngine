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
            [materialIndex](const HedgehogEngine::DrawNode& node) { return node.m_MaterialIndex == materialIndex; });

        if (it == bucket.end())
        {
            HedgehogEngine::DrawNode node;
            node.m_MaterialIndex = materialIndex;
            node.m_Objects.push_back(object);
            bucket.push_back(std::move(node));
        }
        else
        {
            it->m_Objects.push_back(object);
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
        frame.m_DeltaTime = deltaTime;

        frame.m_Camera.m_View     = camera.GetViewMatrix();
        frame.m_Camera.m_Proj     = camera.GetProjectionMatrix();
        frame.m_Camera.m_Position = camera.GetPosition();
        frame.m_Camera.m_Near     = camera.GetNearPlane();
        frame.m_Camera.m_Far      = camera.GetFarPlane();

        const size_t lightCount = std::min(
            lightSystem.GetLightComponentsCount(),
            static_cast<size_t>(MAX_LIGHTS_COUNT));

        frame.m_Lights.reserve(lightCount);

        for (size_t i = 0; i < lightCount; ++i)
        {
            const auto& lc = lightSystem.GetLightComponentByIndex(ecs, i);
            if (!lc.m_Enable)
                continue;

            LightData& ld  = frame.m_Lights.emplace_back();
            ld.m_Position  = lc.m_Position;
            ld.m_Direction = lc.m_Direction;
            ld.m_Color     = lc.m_Color;
            ld.m_Intensity = lc.m_Intensity;
            ld.m_Radius    = lc.m_Radius;
            ld.m_ConeAngle = lc.m_ConeAngle;
            ld.m_Type      = static_cast<int>(lc.m_LightType);
        }

        frame.m_ShadowLightDirection = lightSystem.GetShadowDir();

        BuildDrawList(ecs, renderSystem, materialTypeLookup, frame.m_DrawList);

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
            if (!renderComponent.m_IsVisible || !renderComponent.m_MaterialIndex.has_value())
                continue;

            const auto& meshComponent = ecs.GetComponent<MeshComponent>(entity);
            if (!meshComponent.m_MeshIndex.has_value())
            {
                LOGERROR("Entity ", entity, " has a render component but no mesh index.");
                continue;
            }

            const uint64_t materialIndex = renderComponent.m_MaterialIndex.value();
            const uint64_t meshIndex     = meshComponent.m_MeshIndex.value();
            const auto&    transform     = ecs.GetComponent<TransformComponent>(entity);

            DrawObject object;
            object.m_MeshIndex = meshIndex;
            object.m_Transform = transform.m_ObjMatrix;

            const MaterialType matType = materialTypeLookup(materialIndex);

            switch (matType)
            {
            case MaterialType::Opaque:
                AddToDrawBucket(outDrawList.m_Opaque, object, materialIndex);
                break;
            case MaterialType::Cutoff:
                AddToDrawBucket(outDrawList.m_Cutoff, object, materialIndex);
                break;
            case MaterialType::Transparent:
                AddToDrawBucket(outDrawList.m_Transparent, object, materialIndex);
                break;
            default:
                LOGERROR("Unknown material type for material index ", materialIndex);
                break;
            }
        }
    }
}
