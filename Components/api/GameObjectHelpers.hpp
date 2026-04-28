#pragma once

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "Components/api/HierarchyComponent.hpp"
#include "Components/api/TransformComponent.hpp"

#include <optional>
#include <string>

namespace Scene
{
    class HierarchySystem;
}

namespace Components
{
    ECS::Entity CreateSceneRoot(ECS::ECS& ecs, Scene::HierarchySystem& hierarchySystem);

    ECS::Entity CreateGameObject(ECS::ECS& ecs, ECS::Entity root,
                                  std::optional<ECS::Entity> parent = std::nullopt);

    void DeleteGameObject(ECS::ECS& ecs, ECS::Entity entity);
    void DeleteGameObjectAndChildren(ECS::ECS& ecs, ECS::Entity entity);

    std::string GetUniqueGameObjectName();
}
