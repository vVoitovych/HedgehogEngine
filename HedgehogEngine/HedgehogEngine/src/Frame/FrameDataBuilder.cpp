#include "FrameDataBuilder.hpp"

#include "HedgehogCommon/api/Camera.hpp"
#include "HedgehogCommon/api/RendererSettings.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/CameraSystem.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"

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
        const CameraSystem&                          cameraSystem,
        const Camera&                                camera,
        float                                        gameAspectRatio,
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

        frame.GameCamera = BuildGameCamera(ecs, cameraSystem, gameAspectRatio);

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

    std::optional<CameraData> FrameDataBuilder::BuildGameCamera(
        const ECS::ECS&     ecs,
        const CameraSystem& cameraSystem,
        float               aspectRatio) const
    {
        const CameraComponent* primary       = nullptr;
        ECS::Entity            primaryEntity  = 0;
        size_t                 primaryCount   = 0;

        for (auto entity : cameraSystem.GetEntities())
        {
            const auto& cam = ecs.GetComponent<CameraComponent>(entity);
            if (!cam.IsPrimary || !ecs.HasComponent<TransformComponent>(entity))
                continue;

            ++primaryCount;
            if (primary == nullptr)
            {
                primary       = &cam;
                primaryEntity = entity;
            }
        }

        if (primary == nullptr)
            return std::nullopt;

        if (primaryCount > 1)
            LOGWARNING("Multiple primary cameras found; using the first (entity ", primaryEntity, ").");

        const HM::Matrix4x4& world = ecs.GetComponent<TransformComponent>(primaryEntity).ObjMatrix;

        // Row-vector convention (see LightSystem/TransformSystem): row 0 = forward (local +X),
        // row 2 = up (local +Z), row 3 = world translation.
        const HM::Vector3 eye     = { world[3][0], world[3][1], world[3][2] };
        const HM::Vector3 forward = { world[0][0], world[0][1], world[0][2] };
        const HM::Vector3 up      = { world[2][0], world[2][1], world[2][2] };

        const float aspect = (aspectRatio > 0.0f) ? aspectRatio : 1.0f;

        CameraData data;
        data.View        = HM::Matrix4x4::LookAt(eye, eye + forward, up);
        // Matches the editor Camera's projection setup (fovY = fov/aspect, plus Vulkan Y-flip).
        data.Proj        = HM::Matrix4x4::Perspective(
            HM::ToRadians(primary->Fov) / aspect, aspect, primary->NearPlane, primary->FarPlane);
        data.Proj[1][1] *= -1.0f;
        data.Position    = eye;
        data.Near        = primary->NearPlane;
        data.Far         = primary->FarPlane;
        return data;
    }
}
