#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/Entity.hpp"

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

namespace HedgehogEngine
{
    class EventBus;
    class TransformSystem;
    class MeshSystem;
    class RenderSystem;

    // A whole scene held in memory: what CaptureSnapshot saw and RestoreSnapshot puts back.
    struct SceneSnapshot
    {
        std::string Yaml;            // the scene serialized exactly as SaveScene would write it
        std::string SceneName;
        size_t      GameObjectIndex = 0;
    };

    // Scene / game-object facade: owns the scene name and game-object index, and provides
    // load/save/reset and create/delete game-object operations. Holds non-owning references
    // to the engine services it needs (ECS, event bus, filesystem, systems, serializer registry),
    // all of which outlive it because EngineContext constructs it after them.
    class SceneManager
    {
    public:
        HEDGEHOG_ENGINE_API SceneManager(ECS::ECS& ecs,
                                          EventBus& eventBus,
                                          const FS::FileSystemManager& fileSystem,
                                          EcsSerialization::ComponentSerializerRegistry& componentRegistry,
                                          TransformSystem& transformSystem,
                                          MeshSystem& meshSystem,
                                          RenderSystem& renderSystem);
        HEDGEHOG_ENGINE_API ~SceneManager();

        SceneManager(const SceneManager&)            = delete;
        SceneManager& operator=(const SceneManager&) = delete;
        SceneManager(SceneManager&&)                 = delete;
        SceneManager& operator=(SceneManager&&)      = delete;

        HEDGEHOG_ENGINE_API bool LoadScene(const std::string& filePath);
        HEDGEHOG_ENGINE_API bool SaveScene(const std::string& filePath);
        HEDGEHOG_ENGINE_API void ResetScene();
        HEDGEHOG_ENGINE_API void SetSceneName(const std::string& name);
        HEDGEHOG_ENGINE_API std::string GetSceneName() const;

        // Restore replaces the whole tree with the captured one. Entities come back
        // with their captured ids, so ids held elsewhere (the editor selection,
        // script references) stay valid across a round trip.
        [[nodiscard]] HEDGEHOG_ENGINE_API SceneSnapshot CaptureSnapshot() const;
        [[nodiscard]] HEDGEHOG_ENGINE_API bool          RestoreSnapshot(const SceneSnapshot& snapshot);

        HEDGEHOG_ENGINE_API ECS::Entity CreateGameObject(std::optional<ECS::Entity> parent = std::nullopt);
        HEDGEHOG_ENGINE_API void        DeleteGameObject(ECS::Entity entity);

        HEDGEHOG_ENGINE_API ECS::Entity GetRootEntity() const;

    private:
        void        CreateSceneRoot();
        void        DeleteGameObjectAndChildren(ECS::Entity entity);
        // Brings the systems in line with a freshly deserialized tree.
        void        RefreshAfterLoad();
        std::string GetUniqueGameObjectName();

    private:
        ECS::ECS&                                      m_ECS;
        EventBus&                                       m_EventBus;
        const FS::FileSystemManager&                    m_FileSystem;
        EcsSerialization::ComponentSerializerRegistry&  m_ComponentRegistry;

        TransformSystem& m_TransformSystem;
        MeshSystem&      m_MeshSystem;
        RenderSystem&    m_RenderSystem;

        std::string m_SceneName;
        size_t      m_GameObjectIndex = 0;
    };
}
