#pragma once

#include "HedgehogContext/api/HedgehogContextApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "FrameData/FrameData.hpp"

#include <memory>
#include <string>

namespace Scene
{
    class TransformSystem;
    class HierarchySystem;
    class MeshSystem;
    class LightSystem;
    class RenderSystem;
    class ScriptSystem;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace Context
{
    class WindowContext;
    class Camera;
    class MeshContainer;
    class TextureContainer;
    class LightContainer;
    class MaterialContainer;

    class EngineContext
    {
    public:
        HEDGEHOG_CONTEXT_API EngineContext();
        HEDGEHOG_CONTEXT_API ~EngineContext();

        HEDGEHOG_CONTEXT_API void UpdateContext(WindowContext& windowContext, float aspectRatio, float dt);

        HEDGEHOG_CONTEXT_API const MeshContainer&     GetMeshContainer()     const;
        HEDGEHOG_CONTEXT_API const TextureContainer&  GetTextureContainer()  const;
        HEDGEHOG_CONTEXT_API TextureContainer&        GetTextureContainer();
        HEDGEHOG_CONTEXT_API const LightContainer&    GetLightContainer()    const;
        HEDGEHOG_CONTEXT_API const MaterialContainer& GetMaterialContainer() const;
        HEDGEHOG_CONTEXT_API MaterialContainer&       GetMaterialContainer();

        HEDGEHOG_CONTEXT_API const FD::FrameData&     GetFrameData()         const;

        HEDGEHOG_CONTEXT_API HedgehogSettings::Settings&       GetSettings();
        HEDGEHOG_CONTEXT_API const HedgehogSettings::Settings& GetSettings() const;

        HEDGEHOG_CONTEXT_API const Camera& GetCamera() const;

        HEDGEHOG_CONTEXT_API ECS::ECS&                GetECS();
        HEDGEHOG_CONTEXT_API ECS::Entity              GetRootEntity()      const;
        HEDGEHOG_CONTEXT_API std::string              GetSceneName()       const;
        HEDGEHOG_CONTEXT_API Scene::TransformSystem*  GetTransformSystem() const;
        HEDGEHOG_CONTEXT_API Scene::HierarchySystem*  GetHierarchySystem() const;
        HEDGEHOG_CONTEXT_API Scene::MeshSystem*       GetMeshSystem()      const;
        HEDGEHOG_CONTEXT_API Scene::LightSystem*      GetLightSystem()     const;
        HEDGEHOG_CONTEXT_API Scene::RenderSystem*     GetRenderSystem()    const;
        HEDGEHOG_CONTEXT_API Scene::ScriptSystem*     GetScriptSystem()    const;

        HEDGEHOG_CONTEXT_API void LoadScene(const std::string& filePath);
        HEDGEHOG_CONTEXT_API void SaveScene(const std::string& filePath);
        HEDGEHOG_CONTEXT_API void ResetScene();
        HEDGEHOG_CONTEXT_API void SetSceneName(const std::string& name);

    private:
        void InitECS();
        void UpdateCamera(WindowContext& windowContext, float aspectRatio, float dt);

    private:
        std::unique_ptr<Camera> m_Camera;

        ECS::ECS    m_ECS;
        ECS::Entity m_RootEntity;
        std::string m_SceneName;

        std::shared_ptr<Scene::TransformSystem>  m_TransformSystem;
        std::shared_ptr<Scene::HierarchySystem>  m_HierarchySystem;
        std::shared_ptr<Scene::MeshSystem>       m_MeshSystem;
        std::shared_ptr<Scene::LightSystem>      m_LightSystem;
        std::shared_ptr<Scene::RenderSystem>     m_RenderSystem;
        std::shared_ptr<Scene::ScriptSystem>     m_ScriptSystem;

        std::unique_ptr<MeshContainer>     m_MeshContainer;
        std::unique_ptr<TextureContainer>  m_TextureContainer;
        std::unique_ptr<LightContainer>    m_LightContainer;
        std::unique_ptr<MaterialContainer> m_MaterialContainer;

        FD::FrameData m_FrameData;

        std::unique_ptr<HedgehogSettings::Settings> m_Settings;
    };
}
