#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogEngine/api/Frame/FrameData.hpp"

#include <memory>
#include <optional>
#include <string>

namespace HedgehogSettings
{
    class Settings;
}

namespace EcsSerialization
{
    class ComponentSerializerRegistry;
}

namespace HedgehogEngine
{
    class WindowContext;
    class Camera;
    class MeshContainer;
    class TextureContainer;
    class LightContainer;
    class MaterialContainer;

    class TransformSystem;
    class HierarchySystem;
    class MeshSystem;
    class LightSystem;
    class RenderSystem;
    class ScriptSystem;

    class EngineContext
    {
    public:
        HEDGEHOG_ENGINE_API EngineContext();
        HEDGEHOG_ENGINE_API ~EngineContext();

        HEDGEHOG_ENGINE_API void UpdateContext(WindowContext& windowContext, float aspectRatio, float dt);

        HEDGEHOG_ENGINE_API const MeshContainer&     GetMeshContainer()     const;
        HEDGEHOG_ENGINE_API const TextureContainer&  GetTextureContainer()  const;
        HEDGEHOG_ENGINE_API TextureContainer&        GetTextureContainer();
        HEDGEHOG_ENGINE_API const LightContainer&    GetLightContainer()    const;
        HEDGEHOG_ENGINE_API const MaterialContainer& GetMaterialContainer() const;
        HEDGEHOG_ENGINE_API MaterialContainer&       GetMaterialContainer();

        HEDGEHOG_ENGINE_API const FrameData&         GetFrameData()         const;

        HEDGEHOG_ENGINE_API HedgehogSettings::Settings&       GetSettings();
        HEDGEHOG_ENGINE_API const HedgehogSettings::Settings& GetSettings() const;

        HEDGEHOG_ENGINE_API const Camera& GetCamera() const;

        HEDGEHOG_ENGINE_API ECS::ECS&           GetECS();
        HEDGEHOG_ENGINE_API ECS::Entity         GetRootEntity()      const;
        HEDGEHOG_ENGINE_API std::string         GetSceneName()       const;
        HEDGEHOG_ENGINE_API TransformSystem*    GetTransformSystem() const;
        HEDGEHOG_ENGINE_API HierarchySystem*    GetHierarchySystem() const;
        HEDGEHOG_ENGINE_API MeshSystem*         GetMeshSystem()      const;
        HEDGEHOG_ENGINE_API LightSystem*        GetLightSystem()     const;
        HEDGEHOG_ENGINE_API RenderSystem*       GetRenderSystem()    const;
        HEDGEHOG_ENGINE_API ScriptSystem*       GetScriptSystem()    const;

        HEDGEHOG_ENGINE_API ECS::Entity CreateGameObject(std::optional<ECS::Entity> parent = std::nullopt);
        HEDGEHOG_ENGINE_API void        DeleteGameObject(ECS::Entity entity);

        HEDGEHOG_ENGINE_API void LoadScene(const std::string& filePath);
        HEDGEHOG_ENGINE_API void SaveScene(const std::string& filePath);
        HEDGEHOG_ENGINE_API void ResetScene();
        HEDGEHOG_ENGINE_API void SetSceneName(const std::string& name);

    private:
        void InitECS();
        void RegisterComponents();
        void UpdateCamera(WindowContext& windowContext, float aspectRatio, float dt);

        void        CreateSceneRoot();
        void        DeleteGameObjectAndChildren(ECS::Entity entity);
        static std::string GetUniqueGameObjectName();

    private:
        std::unique_ptr<Camera> m_Camera;

        ECS::ECS    m_ECS;
        std::string m_SceneName;

        std::shared_ptr<TransformSystem>  m_TransformSystem;
        std::shared_ptr<HierarchySystem>  m_HierarchySystem;
        std::shared_ptr<MeshSystem>       m_MeshSystem;
        std::shared_ptr<LightSystem>      m_LightSystem;
        std::shared_ptr<RenderSystem>     m_RenderSystem;
        std::shared_ptr<ScriptSystem>     m_ScriptSystem;

        std::unique_ptr<MeshContainer>     m_MeshContainer;
        std::unique_ptr<TextureContainer>  m_TextureContainer;
        std::unique_ptr<LightContainer>    m_LightContainer;
        std::unique_ptr<MaterialContainer> m_MaterialContainer;

        FrameData m_FrameData;

        std::unique_ptr<HedgehogSettings::Settings>                  m_Settings;
        std::unique_ptr<EcsSerialization::ComponentSerializerRegistry> m_ComponentRegistry;
    };
}
