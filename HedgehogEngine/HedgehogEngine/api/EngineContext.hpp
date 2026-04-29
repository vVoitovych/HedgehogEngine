#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/Entity.hpp"
#include "HedgehogEngine/api/Frame/FrameData.hpp"

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

namespace HedgehogEngine
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
        HEDGEHOG_ENGINE_API EngineContext();
        HEDGEHOG_ENGINE_API ~EngineContext();

        HEDGEHOG_ENGINE_API void UpdateContext(WindowContext& windowContext, float aspectRatio, float dt);

        HEDGEHOG_ENGINE_API const MeshContainer&     GetMeshContainer()     const;
        HEDGEHOG_ENGINE_API const TextureContainer&  GetTextureContainer()  const;
        HEDGEHOG_ENGINE_API TextureContainer&        GetTextureContainer();
        HEDGEHOG_ENGINE_API const LightContainer&    GetLightContainer()    const;
        HEDGEHOG_ENGINE_API const MaterialContainer& GetMaterialContainer() const;
        HEDGEHOG_ENGINE_API MaterialContainer&       GetMaterialContainer();

        HEDGEHOG_ENGINE_API const FD::FrameData&     GetFrameData()         const;

        HEDGEHOG_ENGINE_API HedgehogSettings::Settings&       GetSettings();
        HEDGEHOG_ENGINE_API const HedgehogSettings::Settings& GetSettings() const;

        HEDGEHOG_ENGINE_API const Camera& GetCamera() const;

        HEDGEHOG_ENGINE_API ECS::ECS&                GetECS();
        HEDGEHOG_ENGINE_API ECS::Entity              GetRootEntity()      const;
        HEDGEHOG_ENGINE_API std::string              GetSceneName()       const;
        HEDGEHOG_ENGINE_API Scene::TransformSystem*  GetTransformSystem() const;
        HEDGEHOG_ENGINE_API Scene::HierarchySystem*  GetHierarchySystem() const;
        HEDGEHOG_ENGINE_API Scene::MeshSystem*       GetMeshSystem()      const;
        HEDGEHOG_ENGINE_API Scene::LightSystem*      GetLightSystem()     const;
        HEDGEHOG_ENGINE_API Scene::RenderSystem*     GetRenderSystem()    const;
        HEDGEHOG_ENGINE_API Scene::ScriptSystem*     GetScriptSystem()    const;

        HEDGEHOG_ENGINE_API void LoadScene(const std::string& filePath);
        HEDGEHOG_ENGINE_API void SaveScene(const std::string& filePath);
        HEDGEHOG_ENGINE_API void ResetScene();
        HEDGEHOG_ENGINE_API void SetSceneName(const std::string& name);

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
