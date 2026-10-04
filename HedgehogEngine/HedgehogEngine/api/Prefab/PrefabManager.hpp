#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/Entity.hpp"

#include "EcsSerialization/api/Prefab/IPrefabProvider.hpp"
#include "EcsSerialization/api/Prefab/OverrideSet.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ECS
{
    class ECS;
}

namespace FS
{
    class FileSystemManager;
}

namespace EcsSerialization
{
    class ComponentSerializerRegistry;
}

namespace YAML
{
    class Node;
}

namespace HedgehogEngine
{
    class SceneManager;

    // Prefab assets (epic HE-185): .prefab files of EcsSerialization's PrefabDocument format.
    // EngineContext owns one (GetPrefabs). Paths are virtual (NormalizePrefabPath: backslashes to
    // slashes, no mount meaning assets://) and must end in .prefab. It is the registry's prefab
    // provider while it lives, so scenes, snapshots and saves keep instances as references.
    class PrefabManager : public EcsSerialization::IPrefabProvider
    {
    public:
        HEDGEHOG_ENGINE_API PrefabManager(ECS::ECS& ecs, const FS::FileSystemManager& fileSystem,
                                          EcsSerialization::ComponentSerializerRegistry& registry,
                                          SceneManager& sceneManager);
        HEDGEHOG_ENGINE_API ~PrefabManager() override;

        PrefabManager(const PrefabManager&)            = delete;
        PrefabManager& operator=(const PrefabManager&) = delete;

        // Writes entity and its descendants as a prefab at virtualPath. False, logged, for the
        // scene root, an entity without a hierarchy, a path that is not a .prefab or a failed write.
        HEDGEHOG_ENGINE_API bool CreatePrefab(ECS::Entity entity, const std::string& virtualPath);

        // A new instance of the prefab under parent (the scene root by default): fresh entities,
        // references inside the prefab pointing at the copies and none outside it, and every
        // entity given a PrefabInstanceComponent. The document is cached per path and read again
        // once the file's write time changes. INVALID_ENTITY, logged with the path, for a missing
        // or unreadable file, a document that does not read or an instance that does not fit.
        HEDGEHOG_ENGINE_API ECS::Entity Instantiate(const std::string& virtualPath,
                                                    std::optional<ECS::Entity> parent = std::nullopt);

        // Makes entity and its descendants an instance of the prefab at virtualPath, as
        // Instantiate leaves its entities: each gets a PrefabInstanceComponent (replacing any it
        // had) with its local id in depth-first order, the order a prefab's document numbers its
        // nodes, entity as the instance root, and the path on entity alone. The editor links an
        // entity to the prefab just made from it this way. False, logged, for a path that is not
        // a .prefab or an entity that is not a game object.
        HEDGEHOG_ENGINE_API bool LinkInstance(ECS::Entity entity, const std::string& virtualPath);

        [[nodiscard]] HEDGEHOG_ENGINE_API size_t GetCachedPrefabCount() const;

        // The root of the instance entity belongs to, or INVALID_ENTITY when no prefab made it.
        [[nodiscard]] HEDGEHOG_ENGINE_API ECS::Entity GetInstanceRoot(ECS::Entity entity) const;
        // The overrides of the instance entity belongs to, every node of it, against its prefab
        // (as a scene would save them); empty for an entity of no instance or an unreadable prefab.
        [[nodiscard]] HEDGEHOG_ENGINE_API EcsSerialization::OverrideSet GetOverrides(ECS::Entity entity);

        // Puts the prefab's values back on the instance entity belongs to: a property gets the
        // prefab node's value, and a component the instance added is removed. False, logged, when
        // the entity is of no instance or the prefab does not read.
        HEDGEHOG_ENGINE_API bool Revert(ECS::Entity entity, const EcsSerialization::OverrideSet& overrides);
        // Writes the instance's values into the prefab file (from any node of the instance) and
        // gives them to every other instance of the prefab in the scene that does not override them
        // itself. False, logged, when the entity is of no instance, the prefab does not read or the
        // file cannot be written.
        HEDGEHOG_ENGINE_API bool Apply(ECS::Entity entity, const EcsSerialization::OverrideSet& overrides);

        // EcsSerialization::IPrefabProvider, over PrefabInstanceComponent and the cache.
        [[nodiscard]] HEDGEHOG_ENGINE_API std::optional<EcsSerialization::PrefabLink> GetLink(const ECS::ECS& ecs,
                                                                                            ECS::Entity entity) const override;
        [[nodiscard]] HEDGEHOG_ENGINE_API const char* GetLinkComponentKey() const override;
        [[nodiscard]] HEDGEHOG_ENGINE_API std::shared_ptr<const YAML::Node> LoadPrefab(const std::string& path) override;
        HEDGEHOG_ENGINE_API void Link(ECS::ECS& ecs, const std::vector<ECS::Entity>& entities, const std::string& path) override;

        // The virtual path a prefab is known by, or empty for one that is not a .prefab.
        [[nodiscard]] HEDGEHOG_ENGINE_API static std::string NormalizePrefabPath(const std::string& path);

    private:
        struct CachedPrefab
        {
            std::shared_ptr<const YAML::Node> Root;
            std::filesystem::file_time_type   WriteTime;
        };

        // The prefab's subtree document, read again when its file changed; nullptr, logged, when
        // it cannot be read.
        std::shared_ptr<const YAML::Node> Load(const std::string& virtualPath);

        // Links instanceRoot and its descendants to the prefab at path, local ids in depth-first order.
        void LinkSubtree(ECS::Entity instanceRoot, const std::string& path);

        // The roots of every instance of the prefab at path in the scene.
        std::vector<ECS::Entity> FindInstances(const std::string& path) const;
        // Writes value (one property, or a whole component when property is empty) onto entity.
        void SetValue(ECS::Entity entity, const EcsSerialization::PropertyOverride& value);

    private:
        ECS::ECS&                                      m_ECS;
        const FS::FileSystemManager&                   m_FileSystem;
        EcsSerialization::ComponentSerializerRegistry& m_Registry;
        SceneManager&                                        m_SceneManager;

        std::map<std::string, CachedPrefab> m_Cache;
    };
}
