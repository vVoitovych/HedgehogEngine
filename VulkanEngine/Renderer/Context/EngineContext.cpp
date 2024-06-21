#include "EngineContext.hpp"
#include "VulkanContext.hpp"

#include "Renderer/Containers/MeshContainer.hpp"
#include "Renderer/Containers/TextureContainer.hpp"
#include "Renderer/Containers/LightContainer.hpp"
#include "Renderer/Containers/MaterialContainer.hpp"
#include "Renderer/Camera/Camera.hpp"
#include "Scene/Scene.hpp"

#include "Renderer/WindowManagment/WindowManager.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"

namespace Renderer
{
    EngineContext::EngineContext(const VulkanContext& vulkanContext)
    {
        mCamera = std::make_unique<Camera>();
        mScene = std::make_unique<Scene::Scene>();
        mScene->InitScene();

        mMeshContainer = std::make_unique<MeshContainer>();
        for (auto& mesh : mScene->GetMeshes())
        {
            mMeshContainer->AddFilePath(mesh);
        }
        mMeshContainer->LoadMeshData();
        mMeshContainer->Initialize(vulkanContext);

        mTextureContainer = std::make_unique<TextureContainer>();
        mLightContainer = std::make_unique<LightContainer>();
        mLightContainer->UpdateLights(*mScene);
    }

    EngineContext::~EngineContext()
    {
    }

    void EngineContext::Cleanup(const VulkanContext& vulkanContext)
    {
        mMeshContainer->Cleanup(vulkanContext);
        mTextureContainer->Cleanup(vulkanContext.GetDevice());
    }

    void EngineContext::UpdateContext(VulkanContext& vulkanContext, float dt)
    {
        auto& windowManager = vulkanContext.GetWindowManager();
        auto& controls = windowManager.GetControls();
        const auto& swapChain = vulkanContext.GetSwapChain();
        auto extend = swapChain.GetSwapChainExtent();
        mCamera->UpdateCamera(dt, extend.width / (float)extend.height, controls);
        mScene->UpdateScene(dt);
        mLightContainer->UpdateLights(*mScene);
    }

    const MeshContainer& EngineContext::GetMeshContainer() const
    {
        return *mMeshContainer;
    }

    const TextureContainer& EngineContext::GetTextureContainer() const
    {
        return *mTextureContainer;
    }

    const LightContainer& EngineContext::GetLightContainer() const
    {
        return *mLightContainer;
    }

    const MaterialContainer& EngineContext::GetMaterialContainer() const
    {
        return *mMaterialContainer;
    }

    const Camera& EngineContext::GetCamera() const
    {
        return *mCamera;
    }

    Scene::Scene& EngineContext::GetScene()
    {
        return *mScene;
    }

    const Scene::Scene& EngineContext::GetScene() const
    {
        return *mScene;
    }

}

