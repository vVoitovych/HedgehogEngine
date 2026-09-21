#pragma once

#include "ECS/api/ECS.hpp"
#include "HedgehogEngine/api/ECS/components/RenderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/CameraSystem.hpp"

#include <memory>

namespace HXTest
{
    // A minimal ECS wired up with just the components/systems SceneExtractor reads — no
    // hierarchy, scripting, or transform propagation. Tests set TransformComponent::ObjMatrix
    // and LightComponent::Position/Direction directly, matching how the real systems leave
    // them once a frame has run.
    struct ExtractionFixture
    {
        ECS::ECS ecs;
        std::shared_ptr<HedgehogEngine::RenderSystem> renderSystem;
        std::shared_ptr<HedgehogEngine::LightSystem>  lightSystem;
        std::shared_ptr<HedgehogEngine::CameraSystem>  cameraSystem;

        ExtractionFixture()
        {
            ecs.Init();

            ecs.RegisterComponent<HedgehogEngine::RenderComponent>();
            ecs.RegisterComponent<HedgehogEngine::MeshComponent>();
            ecs.RegisterComponent<HedgehogEngine::TransformComponent>();
            ecs.RegisterComponent<HedgehogEngine::LightComponent>();
            ecs.RegisterComponent<HedgehogEngine::CameraComponent>();

            renderSystem = ecs.RegisterSystem<HedgehogEngine::RenderSystem>();
            lightSystem  = ecs.RegisterSystem<HedgehogEngine::LightSystem>();
            cameraSystem = ecs.RegisterSystem<HedgehogEngine::CameraSystem>();

            ECS::Signature signature;

            signature.set(ecs.GetComponentType<HedgehogEngine::RenderComponent>());
            ecs.SetSystemSignature<HedgehogEngine::RenderSystem>(signature);
            signature.reset();

            signature.set(ecs.GetComponentType<HedgehogEngine::LightComponent>());
            ecs.SetSystemSignature<HedgehogEngine::LightSystem>(signature);
            signature.reset();

            signature.set(ecs.GetComponentType<HedgehogEngine::CameraComponent>());
            ecs.SetSystemSignature<HedgehogEngine::CameraSystem>(signature);
        }

        // A fully-populated, visible renderable entity: RenderComponent + MeshComponent +
        // TransformComponent, with MaterialIndex/MeshIndex already resolved (as they would be
        // by the time a real frame extracts).
        ECS::Entity AddRenderableEntity(uint64_t meshIndex, uint64_t materialIndex, uint32_t layer,
                                         const HM::Matrix4x4& worldMatrix)
        {
            const ECS::Entity entity = ecs.CreateEntity();

            HedgehogEngine::RenderComponent renderComponent;
            renderComponent.IsVisible     = true;
            renderComponent.Layer         = layer;
            renderComponent.MaterialIndex = materialIndex;
            ecs.AddComponent(entity, renderComponent);

            HedgehogEngine::MeshComponent meshComponent;
            meshComponent.MeshIndex = meshIndex;
            ecs.AddComponent(entity, meshComponent);

            HedgehogEngine::TransformComponent transformComponent;
            transformComponent.ObjMatrix = worldMatrix;
            ecs.AddComponent(entity, transformComponent);

            return entity;
        }

        ECS::Entity AddLightEntity(HedgehogEngine::LightType type, const HM::Vector3& position,
                                    const HM::Vector3& direction)
        {
            const ECS::Entity entity = ecs.CreateEntity();

            HedgehogEngine::LightComponent lightComponent;
            lightComponent.LightType = type;
            lightComponent.Position  = position;
            lightComponent.Direction = direction;
            ecs.AddComponent(entity, lightComponent);

            return entity;
        }

        ECS::Entity AddCameraEntity(const HM::Matrix4x4& worldMatrix)
        {
            const ECS::Entity entity = ecs.CreateEntity();

            ecs.AddComponent(entity, HedgehogEngine::CameraComponent{});

            HedgehogEngine::TransformComponent transformComponent;
            transformComponent.ObjMatrix = worldMatrix;
            ecs.AddComponent(entity, transformComponent);

            return entity;
        }
    };
}
