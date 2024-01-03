#include "RenderContext.hpp"

#include "EngineContext.hpp"
#include "FrameContext.hpp"
#include "ThreadContext.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "VulkanEngine/Renderer/WindowManagment/WindowManager.hpp"

#include <stdexcept>

namespace Renderer
{
    RenderContext::RenderContext(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain, std::unique_ptr<WindowManager>&& windowManager)
    {
        mEngineContext = std::make_unique<EngineContext>(device, swapChain, std::move(windowManager));
        mFrameContext = std::make_unique<FrameContext>();
        mThreadContext = std::make_unique<ThreadContext>(device);
    }

    RenderContext::~RenderContext()
    {
    }

    void RenderContext::UpdateContext(float dt)
    {
        mEngineContext->UpdateContext(dt);
        mFrameContext->UpdateContext(mEngineContext->GetCamera());

    }

    void RenderContext::Cleanup(const std::unique_ptr<Device>& device)
    {
        mEngineContext->Cleanup(device);
        mThreadContext->Cleanup(device);
    }

    std::unique_ptr<EngineContext>& RenderContext::GetEngineContext()
    {
        return mEngineContext;
    }

    std::unique_ptr<FrameContext>& RenderContext::GetFrameContext()
    {
        return mFrameContext;
    }

    std::unique_ptr<ThreadContext>& RenderContext::GetThreadContext()
    {
        return mThreadContext;
    }

    const std::unique_ptr<EngineContext>& RenderContext::GetEngineContext() const
    {
        return mEngineContext;
    }

    const std::unique_ptr<FrameContext>& RenderContext::GetFrameContext() const
    {
        return mFrameContext;
    }

    const std::unique_ptr<ThreadContext>& RenderContext::GetThreadContext() const
    {
        return mThreadContext;
    }

    std::tuple<std::unique_ptr<EngineContext>&, std::unique_ptr<FrameContext>&, std::unique_ptr<ThreadContext>&> RenderContext::GetContexts()
    {
        return {GetEngineContext(), GetFrameContext(), GetThreadContext()};
    }

    std::tuple<const std::unique_ptr<EngineContext>&, const std::unique_ptr<FrameContext>&, const std::unique_ptr<ThreadContext>&> RenderContext::GetContexts() const
    {
        return {GetEngineContext(), GetFrameContext(), GetThreadContext()};
    }


}

