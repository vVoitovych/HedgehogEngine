#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include <string>

namespace HedgehogEngine
{
    class ComponentSerializerRegistry;

    class EcsSerializer
    {
    public:
        HEDGEHOG_ENGINE_API static void Serialize(
            const ComponentSerializerRegistry& registry,
            const ECS::ECS& ecs, ECS::Entity root,
            const std::string& sceneName, const std::string& filePath);

        HEDGEHOG_ENGINE_API static void Deserialize(
            const ComponentSerializerRegistry& registry,
            ECS::ECS& ecs, ECS::Entity& outRoot,
            std::string& outSceneName, const std::string& filePath);
    };
}
