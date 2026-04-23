#include "EngineContext.hpp"
#include "WindowContext.hpp"

#include "HedgehogContext/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogContext/Containers/TextureContainer/TextureContainer.hpp"
#include "HedgehogContext/Containers/LightContainer/LightContainer.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialData.hpp"

#include "HedgehogEngine/HedgehogWindow/api/InputState.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "HedgehogCommon/api/Camera.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"

#include "Scene/Scene.hpp"

#include "FrameData/FrameDataBuilder.hpp"

namespace Context
{
    EngineContext::EngineContext()
    {
        m_Camera = std::make_unique<Camera>();
        m_Scene  = std::make_unique<Scene::Scene>();
        m_Scene->InitScene();

        m_MeshContainer = std::make_unique<MeshContainer>();
        m_MeshContainer->Update(*m_Scene);

        m_TextureContainer  = std::make_unique<TextureContainer>();
        m_LightContainer    = std::make_unique<LightContainer>();
        m_LightContainer->UpdateLights(*m_Scene);

        m_MaterialContainer = std::make_unique<MaterialContainer>();

        m_Settings = std::make_unique<HedgehogSettings::Settings>();
    }

    EngineContext::~EngineContext()
    {
    }

    void EngineContext::UpdateContext(WindowContext& windowContext, float aspectRatio, float dt)
    {
        UpdateCamera(windowContext, aspectRatio, dt);
        m_Scene->UpdateScene(dt);
        m_LightContainer->UpdateLights(*m_Scene);
        m_MaterialContainer->Update(*m_Scene);
        m_MeshContainer->Update(*m_Scene);

        auto materialTypeLookup = [this](uint64_t index) -> FD::MaterialType
        {
            const auto& data = m_MaterialContainer->GetMaterialDataByIndex(index);
            return static_cast<FD::MaterialType>(static_cast<int>(data.type));
        };

        FD::FrameDataBuilder builder;
        m_FrameData = builder.Build(*m_Scene, *m_Camera, dt, materialTypeLookup);
    }

    const MeshContainer& EngineContext::GetMeshContainer() const
    {
        return *m_MeshContainer;
    }

    const TextureContainer& EngineContext::GetTextureContainer() const
    {
        return *m_TextureContainer;
    }

    TextureContainer& EngineContext::GetTextureContainer()
    {
        return *m_TextureContainer;
    }

    const LightContainer& EngineContext::GetLightContainer() const
    {
        return *m_LightContainer;
    }

    const MaterialContainer& EngineContext::GetMaterialContainer() const
    {
        return *m_MaterialContainer;
    }

    MaterialContainer& EngineContext::GetMaterialContainer()
    {
        return *m_MaterialContainer;
    }

    const FD::FrameData& EngineContext::GetFrameData() const
    {
        return m_FrameData;
    }

    HedgehogSettings::Settings& EngineContext::GetSettings()
    {
        return *m_Settings;
    }

    const HedgehogSettings::Settings& EngineContext::GetSettings() const
    {
        return *m_Settings;
    }

    const Camera& EngineContext::GetCamera() const
    {
        return *m_Camera;
    }

    Scene::Scene& EngineContext::GetScene()
    {
        return *m_Scene;
    }

    const Scene::Scene& EngineContext::GetScene() const
    {
        return *m_Scene;
    }

    void EngineContext::UpdateCamera(WindowContext& windowContext, float aspectRatio, float dt)
    {
        const auto& inputState = windowContext.GetWindow().GetInputState();

        HM::Vector3 posOffset(0.0f, 0.0f, 0.0f);
        HM::Vector2 dirOffset(inputState.m_MouseDelta.x(), inputState.m_MouseDelta.y());

        if (inputState.m_KeyQ)
            posOffset.z() = -1.0f;
        if (inputState.m_KeyE)
            posOffset.z() = 1.0f;
        if (inputState.m_KeyW)
            posOffset.x() = 1.0f;
        if (inputState.m_KeyS)
            posOffset.x() = -1.0f;
        if (inputState.m_KeyD)
            posOffset.y() = -1.0f;
        if (inputState.m_KeyA)
            posOffset.y() = 1.0f;

        m_Camera->UpdateCamera(dt, aspectRatio, posOffset, dirOffset);
    }
}
