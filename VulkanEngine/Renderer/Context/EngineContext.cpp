#include "EngineContext.hpp"
#include "VulkanContext.hpp"

#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"

namespace Renderer
{
    EngineContext::EngineContext(const std::unique_ptr<VulkanContext>& vulkanContext)
    {
        mMeshContainer.AddFilePath("Models\\viking_room.obj");
        mMeshContainer.LoadMeshData();
        mMeshContainer.Initialize(vulkanContext->GetDevice(), vulkanContext->GetCommandPool());

        mTextureContainer.AddTexture("Textures\\viking_room.png", VK_FORMAT_R8G8B8A8_SRGB);
        mTextureContainer.Initialize(vulkanContext->GetDevice(), vulkanContext->GetCommandPool());

        mSamplerContainer.Initialize(vulkanContext->GetDevice());

        mScene.InitScene();
    }

    void EngineContext::Cleanup(const std::unique_ptr<VulkanContext>& vulkanContext)
    {
        mMeshContainer.Cleanup(vulkanContext->GetDevice());
        mTextureContainer.Cleanup();
        mSamplerContainer.Cleanup(vulkanContext->GetDevice());
    }

    void EngineContext::UpdateContext(const std::unique_ptr<VulkanContext>& vulkanContext, float dt)
    {
        const auto& windowManager = vulkanContext->GetWindowManager();
        const auto& controls = windowManager->GetControls();
        const auto& swapChain = vulkanContext->GetSwapChain();
        auto extend = swapChain->GetSwapChainExtent();
        mCamera.UpdateCamera(dt, extend.width / (float)extend.height, controls);

    }

    const MeshContainer& EngineContext::GetMeshContainer() const
    {
        return mMeshContainer;
    }

    const TextureContaineer& EngineContext::GetTextureContainer() const
    {
        return mTextureContainer;
    }

    const SamplerContainer& EngineContext::GetSamplerContainer() const
    {
        return mSamplerContainer;
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

