#include "api/SceneExtractor.hpp"

#include "api/RenderScene.hpp"

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/CameraSystem.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"

namespace
{
    // The local bounds of a mesh the caller has no bounds for (see RenderInstance::WorldBounds).
    const HM::AABB kUnitLocalBounds(HM::Vector3(-0.5f, -0.5f, -0.5f), HM::Vector3(0.5f, 0.5f, 0.5f));

    HX::LightType ToExtractLightType(HedgehogEngine::LightType type)
    {
        switch (type)
        {
        case HedgehogEngine::LightType::DirectionLight: return HX::LightType::Directional;
        case HedgehogEngine::LightType::PointLight:     return HX::LightType::Point;
        case HedgehogEngine::LightType::SpotLight:      return HX::LightType::Spot;
        }
        return HX::LightType::Directional;
    }

    HX::CameraProjectionType ToExtractProjectionType(HedgehogEngine::CameraProjectionType type)
    {
        switch (type)
        {
        case HedgehogEngine::CameraProjectionType::Perspective:  return HX::CameraProjectionType::Perspective;
        case HedgehogEngine::CameraProjectionType::Orthographic: return HX::CameraProjectionType::Orthographic;
        }
        return HX::CameraProjectionType::Perspective;
    }

    HX::CameraTargetMode ToExtractTargetMode(HedgehogEngine::CameraTargetMode mode)
    {
        switch (mode)
        {
        case HedgehogEngine::CameraTargetMode::Main:    return HX::CameraTargetMode::Main;
        case HedgehogEngine::CameraTargetMode::Texture: return HX::CameraTargetMode::Texture;
        }
        return HX::CameraTargetMode::Main;
    }
}

namespace HX
{
    void SceneExtractor::Extract(
        const ECS::ECS&                     ecs,
        const HedgehogEngine::RenderSystem&  renderSystem,
        const HedgehogEngine::LightSystem&   lightSystem,
        const HedgehogEngine::CameraSystem&  cameraSystem,
        RenderScene&                         outScene,
        std::span<const HM::AABB>            meshLocalBounds) const
    {
        ExtractInstances(ecs, renderSystem, meshLocalBounds, outScene);
        ExtractLights(ecs, lightSystem, outScene);
        ExtractCameras(ecs, cameraSystem, outScene);
    }

    void SceneExtractor::ExtractInstances(const ECS::ECS& ecs, const HedgehogEngine::RenderSystem& renderSystem,
                                           std::span<const HM::AABB> meshLocalBounds, RenderScene& outScene) const
    {
        for (const ECS::Entity entity : renderSystem.GetEntities())
        {
            const auto& renderComponent = ecs.GetComponent<HedgehogEngine::RenderComponent>(entity);
            if (!renderComponent.IsVisible || !renderComponent.MaterialIndex.has_value())
            {
                continue;
            }

            if (!ecs.HasComponent<HedgehogEngine::MeshComponent>(entity) ||
                !ecs.HasComponent<HedgehogEngine::TransformComponent>(entity))
            {
                continue;
            }

            const auto& meshComponent = ecs.GetComponent<HedgehogEngine::MeshComponent>(entity);
            if (!meshComponent.MeshIndex.has_value())
            {
                continue;
            }

            const auto& transformComponent = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity);
            const size_t meshIndex         = static_cast<size_t>(*meshComponent.MeshIndex);
            const HM::AABB& localBounds    = meshIndex < meshLocalBounds.size() ? meshLocalBounds[meshIndex]
                                                                                : kUnitLocalBounds;

            RenderInstance instance;
            instance.WorldMatrix   = transformComponent.ObjMatrix;
            instance.WorldBounds   = localBounds.Transform(transformComponent.ObjMatrix);
            instance.MeshIndex     = *meshComponent.MeshIndex;
            instance.MaterialIndex = *renderComponent.MaterialIndex;
            instance.Layer         = renderComponent.Layer;
            instance.SourceId      = static_cast<uint64_t>(entity);
            outScene.Instances.push_back(instance);
        }
    }

    void SceneExtractor::ExtractLights(const ECS::ECS& ecs, const HedgehogEngine::LightSystem& lightSystem,
                                        RenderScene& outScene) const
    {
        for (const ECS::Entity entity : lightSystem.GetEntities())
        {
            const auto& lightComponent = ecs.GetComponent<HedgehogEngine::LightComponent>(entity);
            if (!lightComponent.Enable)
            {
                continue;
            }

            RenderLight light;
            light.Type        = ToExtractLightType(lightComponent.LightType);
            light.Position    = lightComponent.Position;
            light.Direction   = lightComponent.Direction;
            light.Color       = lightComponent.Color;
            light.Intensity   = lightComponent.Intensity;
            light.Radius      = lightComponent.Radius;
            light.ConeAngle   = lightComponent.ConeAngle;
            light.CastShadows = lightComponent.CastShadows;
            light.SourceId    = static_cast<uint64_t>(entity);
            outScene.Lights.push_back(light);
        }
    }

    void SceneExtractor::ExtractCameras(const ECS::ECS& ecs, const HedgehogEngine::CameraSystem& cameraSystem,
                                         RenderScene& outScene) const
    {
        for (const ECS::Entity entity : cameraSystem.GetEntities())
        {
            const auto& cameraComponent = ecs.GetComponent<HedgehogEngine::CameraComponent>(entity);
            if (!cameraComponent.IsEnabled)
            {
                continue;
            }

            if (!ecs.HasComponent<HedgehogEngine::TransformComponent>(entity))
            {
                continue;
            }

            const auto& transformComponent = ecs.GetComponent<HedgehogEngine::TransformComponent>(entity);

            RenderCamera camera;
            camera.WorldMatrix     = transformComponent.ObjMatrix;
            camera.ProjectionType  = ToExtractProjectionType(cameraComponent.ProjectionType);
            camera.Fov             = cameraComponent.Fov;
            camera.OrthoSize       = cameraComponent.OrthoSize;
            camera.NearPlane       = cameraComponent.NearPlane;
            camera.FarPlane        = cameraComponent.FarPlane;
            camera.LayerMask       = cameraComponent.LayerMask;
            camera.TargetMode      = ToExtractTargetMode(cameraComponent.TargetMode);
            camera.TargetName      = cameraComponent.TargetName;
            camera.GraphName       = cameraComponent.GraphName;
            camera.Priority        = cameraComponent.Priority;
            camera.SourceId        = static_cast<uint64_t>(entity);
            outScene.Cameras.push_back(camera);
        }
    }
}
