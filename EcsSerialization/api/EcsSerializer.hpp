#pragma once

#include "EcsSerializationApi.hpp"
#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include <string>

namespace EcsSerialization
{
    class ComponentSerializerRegistry;

    class EcsSerializer
    {
    public:
        ECS_SERIALIZATION_API static void Serialize(
            const ComponentSerializerRegistry& registry,
            const ECS::ECS& ecs,
            const std::string& sceneName, const std::string& filePath);

        ECS_SERIALIZATION_API static void Deserialize(
            const ComponentSerializerRegistry& registry,
            ECS::ECS& ecs,
            std::string& outSceneName, const std::string& filePath);
    };
}
