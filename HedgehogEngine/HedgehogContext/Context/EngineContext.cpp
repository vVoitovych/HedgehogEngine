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

#include "Scene/Scene.hpp"


namespace Context
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

        mMaterialContainer = std::make_unique<MaterialContainer>(vulkanContext);
        mDrawListContainer = std::make_unique<DrawListContainer>();
    }

    EngineContext::~EngineContext()
    {
    }

    void EngineContext::Cleanup(const VulkanContext& vulkanContext)
    {
        mMeshContainer->Cleanup(vulkanContext);
        mTextureContainer->Cleanup(vulkanContext.GetDevice());
        mMaterialContainer->Cleanup(vulkanContext);
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
        mMaterialContainer->Update(*mScene);
        mMaterialContainer->UpdateResources(vulkanContext, *mTextureContainer);
        mDrawListContainer->Update(*mScene, *mMaterialContainer);
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

    MaterialContainer& EngineContext::GetMaterialContainer()
    {
        return *mMaterialContainer;
    }

    const DrawListContainer& EngineContext::GetDrawListContainer() const
    {
        return *mDrawListContainer;
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

