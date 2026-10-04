#pragma once

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace YAML
{
    class Node;
}

namespace EcsSerialization
{
    // An entity's link to the prefab node it was instantiated from.
    struct PrefabLink
    {
        std::string PrefabPath;   // set on the instance's root only
        uint32_t    LocalId = 0;  // the node's local id, 0 for the root
        ECS::Entity InstanceRoot = ECS::INVALID_ENTITY;
    };

    // What the scene serializer needs to know about prefabs, which live above it (the engine's
    // PrefabManager implements it). Set on the ComponentSerializerRegistry, so scenes, snapshots
    // and saves all write an instance as its prefab's path plus overrides and read it back by
    // instantiating the prefab.
    class IPrefabProvider
    {
    public:
        virtual ~IPrefabProvider() = default;

        // The entity's link, or nullopt for an entity no prefab made.
        [[nodiscard]] virtual std::optional<PrefabLink> GetLink(const ECS::ECS& ecs, ECS::Entity entity) const = 0;
        // The YAML key of the component holding the link, which overrides never include.
        [[nodiscard]] virtual const char* GetLinkComponentKey() const = 0;
        // The prefab's subtree document, or nullptr, logged, when it cannot be read.
        [[nodiscard]] virtual std::shared_ptr<const YAML::Node> LoadPrefab(const std::string& path) = 0;
        // Links the entities a scene load instantiated (index = local id, 0 the root) to the prefab.
        virtual void Link(ECS::ECS& ecs, const std::vector<ECS::Entity>& entities, const std::string& path) = 0;
    };
}
