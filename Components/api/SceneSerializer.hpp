#pragma once

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include <string>

namespace Scene
{
    class HierarchySystem;
    class ScriptSystem;
}

namespace Components
{
    class SceneSerializer
    {
    public:
        static void SerializeScene(const ECS::ECS& ecs, ECS::Entity root,
                                   const std::string& sceneName, const std::string& filePath);

        static void DeserializeScene(ECS::ECS& ecs, ECS::Entity& outRoot,
                                     std::string& outSceneName, const std::string& filePath,
                                     Scene::HierarchySystem& hierarchySystem,
                                     Scene::ScriptSystem& scriptSystem);
    };
}
