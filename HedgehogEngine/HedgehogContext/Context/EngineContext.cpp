#include "EngineContext.hpp"
#include "VulkanContext.hpp"

#include "HedgehogContext/Containers/MeshContainer/MeshContainer.hpp"
#include "HedgehogContext/Containers/TextureContainer/TextureContainer.hpp"
#include "HedgehogContext/Containers/LightContainer/LightContainer.hpp"
#include "HedgehogContext/Containers/MaterialContainer/MaterialContainer.hpp"
#include "HedgehogContext/Containers/DrawListContrainer/DrawListContainer.hpp"

#include "HedgehogWrappers/WindowManagment/WindowManager.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"

#include "HedgehogCommon/Camera/Camera.hpp"

#include "HedgehogSettings/Settings/HedgehogSettings.hpp"

#include "Scene/Scene.hpp"

namespace Context
{
    EngineContext::EngineContext(const VulkanContext& vulkanContext)
    {
        m_Camera = std::make_unique<Camera>();
        m_Scene = std::make_unique<Scene::Scene>();
        m_Scene->InitScene();

        m_MeshContainer = std::make_unique<MeshContainer>();
        m_MeshContainer->Update(vulkanContext, *m_Scene);

        m_TextureContainer = std::make_unique<TextureContainer>();
        m_LightContainer = std::make_unique<LightContainer>();
        m_LightContainer->UpdateLights(*m_Scene);

        m_MaterialContainer = std::make_unique<MaterialContainer>(vulkanContext);
        m_DrawListContainer = std::make_unique<DrawListContainer>();

        m_Settings = std::make_unique<HedgehogSettings::Settings>();
    }

    EngineContext::~EngineContext()
    {
    }

    void EngineContext::Cleanup(const VulkanContext& vulkanContext)
    {
        m_MeshContainer->Cleanup(vulkanContext);
        m_TextureContainer->Cleanup(vulkanContext.GetDevice());
        m_MaterialContainer->Cleanup(vulkanContext);
    }

    void EngineContext::UpdateContext(VulkanContext& vulkanContext, float dt)
    {
        UpdateCamera(vulkanContext, dt);
        m_Scene->UpdateScene(dt);
        m_LightContainer->UpdateLights(*m_Scene);
        m_MaterialContainer->Update(*m_Scene);
        m_MeshContainer->Update(vulkanContext, *m_Scene); 
        m_MaterialContainer->UpdateResources(vulkanContext, *m_TextureContainer);
        m_DrawListContainer->Update(*m_Scene, *m_MaterialContainer);
    }

    const MeshContainer& EngineContext::GetMeshContainer() const
    {
        return *m_MeshContainer;
    }

    const TextureContainer& EngineContext::GetTextureContainer() const
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

    const DrawListContainer& EngineContext::GetDrawListContainer() const
    {
        return *m_DrawListContainer;
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

    void EngineContext::UpdateCamera(VulkanContext& vulkanContext, float dt)
    {
        auto& windowManager = vulkanContext.GetWindowManager();
        auto& controls = windowManager.GetControls();
        const auto& swapChain = vulkanContext.GetSwapChain();

        auto extend = swapChain.GetSwapChainExtent();
        HM::Vector3 posOffset(0.0f, 0.0f, 0.0f);
        HM::Vector2 dirOffset(controls.MouseDelta.x(), controls.MouseDelta.y());

        if (controls.IsPressedQ)
        {
            posOffset.z() = -1.0f;
        }
        if (controls.IsPressedE)
        {
            posOffset.z() = 1.0f;
        }

        if (controls.IsPressedW)
        {
            posOffset.x() = 1.0f;
        }
        if (controls.IsPressedS)
        {
            posOffset.x() = -1.0f;
        }

        if (controls.IsPressedD)
        {
            posOffset.y() = -1.0f;
        }
        if (controls.IsPressedA)
        {
            posOffset.y() = 1.0f;
        }
        m_Camera->UpdateCamera(dt, extend.width / (float)extend.height, posOffset, dirOffset);
    }

}

