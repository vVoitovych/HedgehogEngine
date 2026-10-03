#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "EcsSerialization/api/EcsSerializer.hpp"
#include "EcsSerialization/api/ComponentSerializerRegistry.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <sstream>

namespace HedgehogEngine
{
    SceneManager::SceneManager(ECS::ECS& ecs,
                               EventBus& eventBus,
                               const FS::FileSystemManager& fileSystem,
                               EcsSerialization::ComponentSerializerRegistry& componentRegistry,
                               TransformSystem& transformSystem,
                               MeshSystem& meshSystem,
                               RenderSystem& renderSystem)
        : m_ECS(ecs)
        , m_EventBus(eventBus)
        , m_FileSystem(fileSystem)
        , m_ComponentRegistry(componentRegistry)
        , m_TransformSystem(transformSystem)
        , m_MeshSystem(meshSystem)
        , m_RenderSystem(renderSystem)
        , m_SceneName("Default")
    {
        CreateSceneRoot();
    }

    SceneManager::~SceneManager()
    {
    }

    bool SceneManager::LoadScene(const std::string& filePath)
    {
        const auto virtualPath = m_FileSystem.ToVirtualPath(filePath);
        if (!virtualPath)
        {
            LOGERROR("SceneManager::LoadScene: path is not under any registered mount (path: ", filePath, ")");
            return false;
        }

        if (!m_FileSystem.Exists(*virtualPath))
        {
            LOGERROR("SceneManager::LoadScene: file does not exist (path: ", *virtualPath, ")");
            return false;
        }

        DeleteGameObjectAndChildren(m_ECS.GetRoot());
        const bool loaded = EcsSerialization::EcsSerializer::Deserialize(m_ComponentRegistry, m_ECS, m_SceneName,
                                                                          *virtualPath, m_FileSystem);
        m_ScenePath = *virtualPath;
        RefreshAfterLoad();
        return loaded;
    }

    SceneSnapshot SceneManager::CaptureSnapshot() const
    {
        SceneSnapshot snapshot;
        snapshot.Yaml            = EcsSerialization::EcsSerializer::SerializeToString(m_ComponentRegistry, m_ECS,
                                                                                      m_SceneName);
        snapshot.SceneName       = m_SceneName;
        snapshot.GameObjectIndex = m_GameObjectIndex;
        return snapshot;
    }

    bool SceneManager::RestoreSnapshot(const SceneSnapshot& snapshot)
    {
        DeleteGameObjectAndChildren(m_ECS.GetRoot());

        // DeserializeFromString logs the parse error itself, naming this source.
        std::string sceneName;
        const bool restored = EcsSerialization::EcsSerializer::DeserializeFromString(
            m_ComponentRegistry, m_ECS, sceneName, snapshot.Yaml, "the snapshot of '" + snapshot.SceneName + "'");

        m_SceneName       = snapshot.SceneName;
        m_GameObjectIndex = snapshot.GameObjectIndex;
        RefreshAfterLoad();
        return restored;
    }

    YAML::Node SceneManager::CaptureWorld() const
    {
        return EcsSerialization::EcsSerializer::SerializeToNode(m_ComponentRegistry, m_ECS, m_SceneName);
    }

    bool SceneManager::RestoreWorld(const YAML::Node& world, const std::string& sourceName)
    {
        DeleteGameObjectAndChildren(m_ECS.GetRoot());
        std::string sceneName = m_SceneName;
        const bool  restored  = EcsSerialization::EcsSerializer::DeserializeFromNode(m_ComponentRegistry, m_ECS, sceneName,
                                                                                    world, sourceName);
        m_SceneName = sceneName;
        RefreshAfterLoad();
        return restored;
    }

    void SceneManager::RefreshAfterLoad()
    {
        for (auto entity : m_TransformSystem.GetEntities())
            m_EventBus.Publish(TransformChangedEvent{ entity });
        m_MeshSystem.Update(m_ECS, m_FileSystem);
        m_RenderSystem.UpdateSystem(m_ECS);
    }

    bool SceneManager::SaveScene(const std::string& filePath)
    {
        const std::string sceneName = std::filesystem::path(filePath).stem().string();
        m_SceneName = sceneName;

        const auto virtualPath = m_FileSystem.ToVirtualPath(filePath);
        if (!virtualPath)
        {
            LOGERROR("SceneManager::SaveScene: path is not under any registered mount (path: ", filePath, ")");
            return false;
        }
        m_ScenePath = *virtualPath;
        return EcsSerialization::EcsSerializer::Serialize(
            m_ComponentRegistry, m_ECS, m_SceneName, *virtualPath, m_FileSystem);
    }

    void SceneManager::ResetScene()
    {
        DeleteGameObjectAndChildren(m_ECS.GetRoot());
        CreateSceneRoot();
        m_SceneName       = "Default";
        m_ScenePath.clear();
        m_GameObjectIndex = 0;
    }

    void SceneManager::SetSceneName(const std::string& name)
    {
        m_SceneName = name;
    }

    std::string SceneManager::GetSceneName() const
    {
        return m_SceneName;
    }

    const std::string& SceneManager::GetScenePath() const
    {
        return m_ScenePath;
    }

    ECS::Entity SceneManager::CreateGameObject(std::optional<ECS::Entity> parent)
    {
        ECS::Entity realParent = parent.value_or(m_ECS.GetRoot());

        ECS::Entity entity = m_ECS.CreateEntity();
        m_ECS.AddComponent(entity, TransformComponent{});
        m_ECS.AddComponent(entity, ECS::HierarchyComponent{ GetUniqueGameObjectName(), realParent, {} });
        m_ECS.GetComponent<ECS::HierarchyComponent>(realParent).Children.push_back(entity);

        m_EventBus.Publish(TransformChangedEvent{ entity });
        return entity;
    }

    void SceneManager::DeleteGameObject(ECS::Entity entity)
    {
        assert(entity != m_ECS.GetRoot() && "Cannot delete the root entity.");

        auto& hierarchy                      = m_ECS.GetComponent<ECS::HierarchyComponent>(entity);
        const ECS::Entity parentEntity       = hierarchy.Parent;
        const auto        childrenToReparent = hierarchy.Children;
        auto& parentHierarchy                = m_ECS.GetComponent<ECS::HierarchyComponent>(parentEntity);

        auto it = std::find(parentHierarchy.Children.begin(),
                             parentHierarchy.Children.end(), entity);
        if (it != parentHierarchy.Children.end())
        {
            parentHierarchy.Children.erase(it);
            for (ECS::Entity child : childrenToReparent)
            {
                auto& childHierarchy    = m_ECS.GetComponent<ECS::HierarchyComponent>(child);
                childHierarchy.Parent = parentEntity;
                auto& freshParentHierarchy = m_ECS.GetComponent<ECS::HierarchyComponent>(parentEntity);
                freshParentHierarchy.Children.push_back(child);
            }
        }
        m_ECS.DestroyEntity(entity);
    }

    ECS::Entity SceneManager::GetRootEntity() const
    {
        return m_ECS.GetRoot();
    }

    void SceneManager::CreateSceneRoot()
    {
        ECS::Entity root = m_ECS.CreateEntity();
        m_ECS.AddComponent(root, TransformComponent{});
        m_ECS.AddComponent(root, ECS::HierarchyComponent{ "Root", root, {} });
        m_ECS.SetRoot(root);
        m_EventBus.Publish(TransformChangedEvent{ root });
    }

    void SceneManager::DeleteGameObjectAndChildren(ECS::Entity entity)
    {
        const auto children = m_ECS.GetComponent<ECS::HierarchyComponent>(entity).Children;
        for (ECS::Entity child : children)
            DeleteGameObjectAndChildren(child);
        m_ECS.DestroyEntity(entity);
    }

    std::string SceneManager::GetUniqueGameObjectName()
    {
        std::ostringstream ss;
        ss << "GameObject_" << m_GameObjectIndex++;
        return ss.str();
    }
}
