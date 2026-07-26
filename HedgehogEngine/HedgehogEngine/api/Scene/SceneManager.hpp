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

        // In-memory scene snapshot/restore for the editor's Play mode: capture before entering
        // Play, restore on Stop so a play session never mutates the saved scene.
        HEDGEHOG_ENGINE_API std::string SnapshotScene() const;
        HEDGEHOG_ENGINE_API bool        RestoreScene(const std::string& snapshot);
        HEDGEHOG_ENGINE_API void SetSceneName(const std::string& name);
        HEDGEHOG_ENGINE_API std::string GetSceneName() const;

        HEDGEHOG_ENGINE_API ECS::Entity CreateGameObject(std::optional<ECS::Entity> parent = std::nullopt);
        HEDGEHOG_ENGINE_API void        DeleteGameObject(ECS::Entity entity);

        HEDGEHOG_ENGINE_API ECS::Entity GetRootEntity() const;

    private:
        void        CreateSceneRoot();
        void        DeleteGameObjectAndChildren(ECS::Entity entity);
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
