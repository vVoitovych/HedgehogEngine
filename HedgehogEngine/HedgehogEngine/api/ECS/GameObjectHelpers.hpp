#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "ECS/api/components/Hierarchy.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include <optional>
#include <string>

namespace Scene
{
    class HierarchySystem;
}

namespace Components
{
    HEDGEHOG_ENGINE_API ECS::Entity CreateSceneRoot(ECS::ECS& ecs, Scene::HierarchySystem& hierarchySystem);

    HEDGEHOG_ENGINE_API ECS::Entity CreateGameObject(ECS::ECS& ecs, ECS::Entity root,
                                                      std::optional<ECS::Entity> parent = std::nullopt);

    HEDGEHOG_ENGINE_API void DeleteGameObject(ECS::ECS& ecs, ECS::Entity entity);
    HEDGEHOG_ENGINE_API void DeleteGameObjectAndChildren(ECS::ECS& ecs, ECS::Entity entity);

    HEDGEHOG_ENGINE_API std::string GetUniqueGameObjectName();
}
