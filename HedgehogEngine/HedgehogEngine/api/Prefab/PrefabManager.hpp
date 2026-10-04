#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/Entity.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>

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
    // slashes, no mount meaning assets://) and must end in .prefab.
    class PrefabManager
    {
    public:
        HEDGEHOG_ENGINE_API PrefabManager(ECS::ECS& ecs, const FS::FileSystemManager& fileSystem,
                                          const EcsSerialization::ComponentSerializerRegistry& registry,
                                          SceneManager& sceneManager);
        HEDGEHOG_ENGINE_API ~PrefabManager();

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

        [[nodiscard]] HEDGEHOG_ENGINE_API size_t GetCachedPrefabCount() const;

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

    private:
        ECS::ECS&                                            m_ECS;
        const FS::FileSystemManager&                         m_FileSystem;
        const EcsSerialization::ComponentSerializerRegistry& m_Registry;
        SceneManager&                                        m_SceneManager;

        std::map<std::string, CachedPrefab> m_Cache;
    };
}
