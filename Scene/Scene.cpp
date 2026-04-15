#include "Scene.hpp"
#include "SceneSerializer.hpp"

#include "Scene/SceneComponents/TransformComponent.hpp"
#include "Scene/SceneComponents/HierarchyComponent.hpp"
#include "Scene/SceneComponents/MeshComponent.hpp"
#include "Scene/SceneComponents/LightComponent.hpp"
#include "Scene/SceneComponents/RenderComponent.hpp"
#include "Scene/SceneComponents/ScriptComponent.hpp"
#include "ContentLoader/CommonFunctions.hpp"
#include "Logger/api/Logger.hpp"
#include "DialogueWindows/SceneDialogue/SceneDialogue.hpp"
#include "DialogueWindows/MaterialDialogue/MaterialDialogue.hpp"

#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Scene
{
    Scene::Scene()
        : m_SceneName("Default")
        , m_Root(0)
    {
    }

    void Scene::InitScene()
    {
        m_SceneECS.Init();

        // Register components
        m_SceneECS.RegisterComponent<TransformComponent>();
        m_SceneECS.RegisterComponent<HierarchyComponent>();
        m_SceneECS.RegisterComponent<MeshComponent>();
        m_SceneECS.RegisterComponent<LightComponent>();
        m_SceneECS.RegisterComponent<RenderComponent>();
        m_SceneECS.RegisterComponent<ScriptComponent>();

        // Register systems
        m_TransformSystem = m_SceneECS.RegisterSystem<TransformSystem>();
        m_HierarchySystem = m_SceneECS.RegisterSystem<HierarchySystem>();
        m_MeshSystem      = m_SceneECS.RegisterSystem<MeshSystem>();
        m_LightSystem     = m_SceneECS.RegisterSystem<LightSystem>();
        m_RenderSystem    = m_SceneECS.RegisterSystem<RenderSystem>();
        m_ScriptSystem    = m_SceneECS.RegisterSystem<ScriptSystem>();

        // Bind systems to their required components
        ECS::Signature signature;

        signature.set(m_SceneECS.GetComponentType<TransformComponent>());
        m_SceneECS.SetSystemSignature<TransformSystem>(signature);
        signature.reset();

        signature.set(m_SceneECS.GetComponentType<HierarchyComponent>());
        m_SceneECS.SetSystemSignature<HierarchySystem>(signature);
        signature.reset();

        signature.set(m_SceneECS.GetComponentType<MeshComponent>());
        m_SceneECS.SetSystemSignature<MeshSystem>(signature);
        signature.reset();

        signature.set(m_SceneECS.GetComponentType<LightComponent>());
        m_SceneECS.SetSystemSignature<LightSystem>(signature);
        signature.reset();

        signature.set(m_SceneECS.GetComponentType<RenderComponent>());
        m_SceneECS.SetSystemSignature<RenderSystem>(signature);
        signature.reset();

        signature.set(m_SceneECS.GetComponentType<ScriptComponent>());
        m_SceneECS.SetSystemSignature<ScriptSystem>(signature);

        CreateSceneRoot();
    }

    void Scene::UpdateScene(float dt)
    {
        m_ScriptSystem->Update(m_SceneECS, dt);
        m_TransformSystem->Update(m_SceneECS);
        m_HierarchySystem->Update(m_SceneECS);
        m_LightSystem->Update(m_SceneECS);
    }

    void Scene::ResetScene()
    {
        DeleteGameObjectAndChildren(m_Root);
    }

    void Scene::Load()
    {
        char* path = DialogueWindows::SceneOpenDialogue();
        if (path == nullptr)
        {
            return;
        }
        ResetScene();
        SceneSerializer::DeserializeScene(*this, path);

        m_MeshSystem->Update(m_SceneECS);
        m_RenderSystem->UpdateSystem(m_SceneECS);
    }

    void Scene::Save()
    {
        char* path = DialogueWindows::SceneSaveDialogue();
        if (path == nullptr)
        {
            return;
        }
        SceneSerializer::SerializeScene(*this, path);
    }

    void Scene::RenameScene()
    {
        char* newName = DialogueWindows::SceneRenameDialogue();
        if (newName != nullptr)
            m_SceneName = newName;
    }

    std::string Scene::GetSceneName() const
    {
        return m_SceneName;
    }

    ECS::Entity Scene::CreateGameObject(std::optional<ECS::Entity> parentEntity)
    {
        ECS::Entity realParentEntity = parentEntity.value_or(m_Root);

        auto& rootHierarchy = m_SceneECS.GetComponent<HierarchyComponent>(realParentEntity);
        ECS::Entity entity  = m_SceneECS.CreateEntity();
        m_SceneECS.AddComponent(entity, TransformComponent{});
        m_SceneECS.AddComponent(entity, HierarchyComponent{ GetNewGameObjectName(), realParentEntity, {} });
        rootHierarchy.m_Children.push_back(entity);

        return entity;
    }

    void Scene::CreateGameObject(ECS::Entity entity)
    {
        m_SceneECS.CreateEntity(entity);
        m_SceneECS.AddComponent(entity, TransformComponent{});
        m_SceneECS.AddComponent(entity, HierarchyComponent{});
    }

    void Scene::DeleteGameObject(ECS::Entity entity)
    {
        auto& hierarchy       = m_SceneECS.GetComponent<HierarchyComponent>(entity);
        auto& parentHierarchy = m_SceneECS.GetComponent<HierarchyComponent>(hierarchy.m_Parent);
        auto  it              = std::find(parentHierarchy.m_Children.begin(),
                                         parentHierarchy.m_Children.end(), entity);
        if (it != parentHierarchy.m_Children.end())
        {
            parentHierarchy.m_Children.erase(it);
            for (ECS::Entity child : hierarchy.m_Children)
            {
                auto& childHierarchy   = m_SceneECS.GetComponent<HierarchyComponent>(child);
                childHierarchy.m_Parent = hierarchy.m_Parent;
                parentHierarchy.m_Children.push_back(child);
            }
        }
        m_SceneECS.DestroyEntity(entity);
    }

    void Scene::DeleteGameObjectAndChildren(ECS::Entity entity)
    {
        auto& hierarchy = m_SceneECS.GetComponent<HierarchyComponent>(entity);
        for (ECS::Entity child : hierarchy.m_Children)
        {
            DeleteGameObjectAndChildren(child);
        }
        m_SceneECS.DestroyEntity(entity);
    }

    void Scene::AddMeshComponent(ECS::Entity entity)
    {
        if (!HasMeshComponent(entity))
        {
            m_SceneECS.AddComponent(entity, MeshComponent{ MeshSystem::sDefaultMeshPath });
            m_MeshSystem->Update(m_SceneECS, entity);
        }
    }

    void Scene::RemoveMeshComponent(ECS::Entity entity)
    {
        if (HasMeshComponent(entity))
        {
            m_SceneECS.RemoveComponent<MeshComponent>(entity);
        }
    }

    void Scene::ChangeMeshComponent(ECS::Entity entity, const std::string& meshPath)
    {
        auto& meshComponent      = m_SceneECS.GetComponent<MeshComponent>(entity);
        meshComponent.m_MeshPath = meshPath;
        m_MeshSystem->Update(m_SceneECS, entity);
    }

    bool Scene::HasMeshComponent(ECS::Entity entity) const
    {
        return m_SceneECS.HasComponent<MeshComponent>(entity);
    }

    void Scene::LoadMesh(ECS::Entity entity)
    {
        m_MeshSystem->LoadMesh(m_SceneECS, entity);
    }

    void Scene::AddRenderComponent(ECS::Entity entity)
    {
        if (!HasRenderComponent(entity))
        {
            m_SceneECS.AddComponent(entity, RenderComponent());
            m_RenderSystem->Update(m_SceneECS, entity);
        }
    }

    void Scene::RemoveRenderComponent(ECS::Entity entity)
    {
        if (HasRenderComponent(entity))
        {
            m_SceneECS.RemoveComponent<RenderComponent>(entity);
        }
    }

    bool Scene::HasRenderComponent(ECS::Entity entity) const
    {
        return m_SceneECS.HasComponent<RenderComponent>(entity);
    }

    void Scene::LoadMaterial(ECS::Entity entity)
    {
        char* path = DialogueWindows::MaterialOpenDialogue();
        if (path == nullptr)
        {
            return;
        }
        std::string relatedPath                      = ContentLoader::GetAssetRelativetlyPath(path);
        auto& component                              = m_SceneECS.GetComponent<RenderComponent>(entity);
        component.m_Material                         = relatedPath;
        m_RenderSystem->Update(m_SceneECS, entity);
    }

    void Scene::UpdateMaterialComponent(ECS::Entity entity)
    {
        m_RenderSystem->Update(m_SceneECS, entity);
    }

    void Scene::AddScriptComponent(ECS::Entity entity)
    {
        if (!HasScriptComponent(entity))
        {
            m_SceneECS.AddComponent(entity, ScriptComponent());
        }
    }

    void Scene::RemoveScriptComponent(ECS::Entity entity)
    {
        if (HasScriptComponent(entity))
        {
            m_SceneECS.RemoveComponent<ScriptComponent>(entity);
        }
    }

    void Scene::ChangeScript(ECS::Entity entity)
    {
        m_ScriptSystem->ChangeScript(entity, m_SceneECS);
    }

    bool Scene::HasScriptComponent(ECS::Entity entity)
    {
        return m_SceneECS.HasComponent<ScriptComponent>(entity);
    }

    void Scene::InitScriptComponent(ECS::Entity entity)
    {
        m_ScriptSystem->InitScript(entity, m_SceneECS);
    }

    void Scene::AddLightComponent(ECS::Entity entity)
    {
        if (!HasLightComponent(entity))
        {
            m_SceneECS.AddComponent(entity, LightComponent());
        }
    }

    void Scene::RemoveLightComponent(ECS::Entity entity)
    {
        if (HasLightComponent(entity))
        {
            m_SceneECS.RemoveComponent<LightComponent>(entity);
        }
    }

    bool Scene::HasLightComponent(ECS::Entity entity) const
    {
        return m_SceneECS.HasComponent<LightComponent>(entity);
    }

    size_t Scene::GetLightCount() const
    {
        return m_LightSystem->GetLightComponentsCount();
    }

    const LightComponent& Scene::GetLightComponentByIndex(size_t index) const
    {
        return m_LightSystem->GetLightComponentByIndex(m_SceneECS, index);
    }

    void Scene::UpdateShadowCasting(ECS::Entity entity, bool isCast)
    {
        m_LightSystem->SetShadowCasting(m_SceneECS, entity, isCast);
    }

    ECS::Entity Scene::GetRoot() const
    {
        return m_Root;
    }

    HierarchyComponent& Scene::GetHierarchyComponent(ECS::Entity entity) const
    {
        return m_SceneECS.GetComponent<HierarchyComponent>(entity);
    }

    TransformComponent& Scene::GetTransformComponent(ECS::Entity entity) const
    {
        return m_SceneECS.GetComponent<TransformComponent>(entity);
    }

    MeshComponent& Scene::GetMeshComponent(ECS::Entity entity) const
    {
        return m_SceneECS.GetComponent<MeshComponent>(entity);
    }

    LightComponent& Scene::GetLightComponent(ECS::Entity entity) const
    {
        return m_SceneECS.GetComponent<LightComponent>(entity);
    }

    RenderComponent& Scene::GetRenderComponent(ECS::Entity entity) const
    {
        return m_SceneECS.GetComponent<RenderComponent>(entity);
    }

    ScriptComponent& Scene::GetScriptComponent(ECS::Entity entity) const
    {
        return m_SceneECS.GetComponent<ScriptComponent>(entity);
    }

    const std::vector<std::string>& Scene::GetMeshes() const
    {
        return m_MeshSystem->GetMeshes();
    }

    const std::vector<std::string>& Scene::GetMaterials() const
    {
        return m_RenderSystem->GetMaterials();
    }

    const std::vector<ECS::Entity>& Scene::GetRenderableEntities() const
    {
        return m_RenderSystem->GetEntities();
    }

    const std::optional<HM::Vector3>& Scene::GetShadowLightDirection() const
    {
        return m_LightSystem->GetShadowDir();
    }

    void Scene::CreateSceneRoot()
    {
        m_Root = m_SceneECS.CreateEntity();
        m_SceneECS.AddComponent(m_Root, TransformComponent{});
        m_SceneECS.AddComponent(m_Root, HierarchyComponent{ "Root", m_Root, {} });
        m_HierarchySystem->SetRoot(m_Root);
    }

    std::string Scene::GetNewGameObjectName()
    {
        static size_t s_Index = 0;
        std::ostringstream ss;
        ss << "GameObject_" << s_Index++;
        return ss.str();
    }

    std::string Scene::GetScenePath() const
    {
        return ContentLoader::GetAssetsDirectory() + "Scenes/" + m_SceneName;
    }
}
