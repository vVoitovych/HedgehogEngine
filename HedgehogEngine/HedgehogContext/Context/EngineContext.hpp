#pragma once

#include "HedgehogContext/api/HedgehogContextApi.hpp"

#include "FrameData/FrameData.hpp"

#include <memory>

namespace Scene
{
    class Scene;
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

        HEDGEHOG_CONTEXT_API const Camera&       GetCamera() const;
        HEDGEHOG_CONTEXT_API Scene::Scene&       GetScene();
        HEDGEHOG_CONTEXT_API const Scene::Scene& GetScene() const;

    private:
        void UpdateCamera(WindowContext& windowContext, float aspectRatio, float dt);

    private:
        std::unique_ptr<Camera>       m_Camera;
        std::unique_ptr<Scene::Scene> m_Scene;

        std::unique_ptr<MeshContainer>     m_MeshContainer;
        std::unique_ptr<TextureContainer>  m_TextureContainer;
        std::unique_ptr<LightContainer>    m_LightContainer;
        std::unique_ptr<MaterialContainer> m_MaterialContainer;

        FD::FrameData m_FrameData;

        std::unique_ptr<HedgehogSettings::Settings> m_Settings;
    };
}
