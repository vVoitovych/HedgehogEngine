#pragma once

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
        EngineContext();
        ~EngineContext();

        void UpdateContext(WindowContext& windowContext, float aspectRatio, float dt);

        const MeshContainer&     GetMeshContainer()     const;
        const TextureContainer&  GetTextureContainer()  const;
        TextureContainer&        GetTextureContainer();
        const LightContainer&    GetLightContainer()    const;
        const MaterialContainer& GetMaterialContainer() const;
        MaterialContainer&       GetMaterialContainer();

        const FD::FrameData&     GetFrameData()         const;

        HedgehogSettings::Settings&       GetSettings();
        const HedgehogSettings::Settings& GetSettings() const;

        const Camera&       GetCamera() const;
        Scene::Scene&       GetScene();
        const Scene::Scene& GetScene() const;

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
