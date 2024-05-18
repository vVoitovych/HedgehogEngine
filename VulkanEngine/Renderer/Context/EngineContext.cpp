#include "EngineContext.hpp"
#include "VulkanContext.hpp"

#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"

namespace Renderer
{
    EngineContext::EngineContext(const VulkanContext& vulkanContext)
    {
        mScene.InitScene();
        for (auto& mesh : mScene.GetMeshes())
        {
            mMeshContainer.AddFilePath(mesh);
        }

        mMeshContainer.LoadMeshData();
        mMeshContainer.Initialize(vulkanContext);

        for (auto texture : mScene.GetTextures())
        {
            mTextureContainer.AddTexture(texture, VK_FORMAT_R8G8B8A8_SRGB);
        }
        mTextureContainer.Initialize(vulkanContext.GetDevice());

        mLightContainer.UpdateLights(mScene);
    }

    void EngineContext::Cleanup(const VulkanContext& vulkanContext)
    {
        mMeshContainer.Cleanup(vulkanContext);
        mTextureContainer.Cleanup(vulkanContext.GetDevice());
    }

    void EngineContext::UpdateContext(VulkanContext& vulkanContext, float dt)
    {
        auto& windowManager = vulkanContext.GetWindowManager();
        auto& controls = windowManager.GetControls();
        const auto& swapChain = vulkanContext.GetSwapChain();
        auto extend = swapChain.GetSwapChainExtent();
        mCamera.UpdateCamera(dt, extend.width / (float)extend.height, controls);
        mScene.UpdateScene(dt);
        mLightContainer.UpdateLights(mScene);
    }

    const MeshContainer& EngineContext::GetMeshContainer() const
    {
        return mMeshContainer;
    }

    const TextureContaineer& EngineContext::GetTextureContainer() const
    {
        return mTextureContainer;
    }

    const LightContainer& EngineContext::GetLightContainer() const
    {
        return mLightContainer;
    }

    const Camera& EngineContext::GetCamera() const
    {
        return mCamera;
    }
    Scene::Scene& EngineContext::GetScene()
    {
        return mScene;
    }
    const Scene::Scene& EngineContext::GetScene() const
    {
        return mScene;
    }
}

