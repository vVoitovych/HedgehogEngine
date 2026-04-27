#pragma once

#include "Scene/api/SceneApi.hpp"

#include "ECS/api/ECS.hpp"
#include "Scene/SceneSystems/TransformSystem.hpp"
#include "Scene/SceneSystems/HierarchySystem.hpp"
#include "Scene/SceneSystems/MeshSystem.hpp"
#include "Scene/SceneSystems/LightSystem.hpp"
#include "Scene/SceneSystems/RenderSystem.hpp"
#include "Scene/SceneSystems/ScriptSystem.hpp"

#include <vector>
#include <optional>
#include <string>

namespace Scene
{
    class HierarchyComponent;
    class TransformComponent;

    class Scene
    {
    public:
        SCENE_API Scene();
        ~Scene() = default;

        Scene(const Scene&)            = delete;
        Scene(Scene&&)                 = delete;
        Scene& operator=(const Scene&) = delete;
        Scene& operator=(Scene&&)      = delete;

        SCENE_API void InitScene();
        SCENE_API void UpdateScene(float dt);
        SCENE_API void ResetScene();
        SCENE_API void Load();
        SCENE_API void Save();
        SCENE_API void RenameScene();

        SCENE_API std::string GetSceneName() const;

        SCENE_API ECS::Entity CreateGameObject(std::optional<ECS::Entity> parentEntity);
        SCENE_API void        CreateGameObject(ECS::Entity entity);
        SCENE_API void        DeleteGameObject(ECS::Entity entity);
        SCENE_API void        DeleteGameObjectAndChildren(ECS::Entity entity);

        SCENE_API HierarchyComponent& GetHierarchyComponent(ECS::Entity entity) const;
        SCENE_API TransformComponent& GetTransformComponent(ECS::Entity entity) const;

        SCENE_API MeshComponent& GetMeshComponent(ECS::Entity entity) const;
        SCENE_API void           AddMeshComponent(ECS::Entity entity);
        SCENE_API void           RemoveMeshComponent(ECS::Entity entity);
        SCENE_API void           ChangeMeshComponent(ECS::Entity entity, const std::string& meshPath);
        SCENE_API bool           HasMeshComponent(ECS::Entity entity) const;
        SCENE_API void           LoadMesh(ECS::Entity entity);

        SCENE_API RenderComponent& GetRenderComponent(ECS::Entity entity) const;
        SCENE_API void             AddRenderComponent(ECS::Entity entity);
        SCENE_API void             RemoveRenderComponent(ECS::Entity entity);
        SCENE_API bool             HasRenderComponent(ECS::Entity entity) const;
        SCENE_API void             LoadMaterial(ECS::Entity entity);
        SCENE_API void             UpdateMaterialComponent(ECS::Entity entity);

        SCENE_API ScriptComponent& GetScriptComponent(ECS::Entity entity) const;
        SCENE_API void             AddScriptComponent(ECS::Entity entity);
        SCENE_API void             RemoveScriptComponent(ECS::Entity entity);
        SCENE_API void             ChangeScript(ECS::Entity entity);
        SCENE_API bool             HasScriptComponent(ECS::Entity entity);
        SCENE_API void             InitScriptComponent(ECS::Entity entity);

        SCENE_API LightComponent& GetLightComponent(ECS::Entity entity) const;
        SCENE_API void            AddLightComponent(ECS::Entity entity);
        SCENE_API void            RemoveLightComponent(ECS::Entity entity);
        SCENE_API bool            HasLightComponent(ECS::Entity entity) const;

        SCENE_API size_t               GetLightCount() const;
        SCENE_API const LightComponent& GetLightComponentByIndex(size_t index) const;
        SCENE_API void                 UpdateShadowCasting(ECS::Entity entity, bool isCast);

        SCENE_API ECS::Entity GetRoot() const;

        SCENE_API const std::vector<std::string>&  GetMeshes() const;
        SCENE_API const std::vector<std::string>&  GetMaterials() const;
        SCENE_API const std::vector<ECS::Entity>&  GetRenderableEntities() const;
        SCENE_API const std::optional<HM::Vector3>& GetShadowLightDirection() const;

    private:
        void        CreateSceneRoot();
        std::string GetNewGameObjectName();
        std::string GetScenePath() const;

    private:
        std::string m_SceneName;

        ECS::ECS    m_SceneECS;
        ECS::Entity m_Root;

        std::shared_ptr<TransformSystem>  m_TransformSystem;
        std::shared_ptr<HierarchySystem>  m_HierarchySystem;
        std::shared_ptr<MeshSystem>       m_MeshSystem;
        std::shared_ptr<LightSystem>      m_LightSystem;
        std::shared_ptr<RenderSystem>     m_RenderSystem;
        std::shared_ptr<ScriptSystem>     m_ScriptSystem;

        friend class SceneSerializer;
    };
}
