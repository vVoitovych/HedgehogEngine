#include "EngineContext.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"

namespace Renderer
{
    EngineContext::EngineContext(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain, std::unique_ptr<WindowManager>&& windowManager)
    {
        mWindowManager = std::move(windowManager);
        mMeshContainer.AddFilePath("Models\\viking_room.obj");
        mMeshContainer.LoadMeshData();
        mMeshContainer.Initialize(device);
        mExtent = swapChain->GetSwapChainExtend();
    }

    void EngineContext::Cleanup(const std::unique_ptr<Device>& device)
    {
        mMeshContainer.Cleanup(device);
    }

    void EngineContext::HandleInput()
    {
        mWindowManager->HandleInput();
    }

    void EngineContext::UpdateContext(float dt)
    {
        auto controls = mWindowManager->GetControls();
        mCamera.UpdateCamera(dt, mExtent.width / (float)mExtent.height, controls);

    }

    void EngineContext::UpdateBackBufferIdex(uint32_t index) const
    {
        mBackBufferIndex = index;
    }
    uint32_t EngineContext::GetBackBufferIndex() const
    {
        return mBackBufferIndex;
    }
    VkExtent2D EngineContext::GetExtent()
    {
        return mExtent;
    }
    MeshContainer& EngineContext::GetMeshContainer() 
    {
        return mMeshContainer;
    }
    const std::unique_ptr<WindowManager>& EngineContext::GetWindowManager() const
    {
        return mWindowManager;
    }
    bool EngineContext::ShouldClose() const
    {
        return mWindowManager->ShouldClose();
    }
    void EngineContext::ResizeWindow()
    {
        mWindowResized = true;
    }
    bool EngineContext::IsWindowResized()
    {
        return mWindowResized;
    }
    void EngineContext::ResetWindowResizeState(VkExtent2D extent)
    {
        mExtent = extent;
        mWindowResized = false;
    }
    const Camera& EngineContext::GetCamera() const
    {
        return mCamera;
    }
}

