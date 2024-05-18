#include "RenderContext.hpp"

#include "VulkanContext.hpp"
#include "EngineContext.hpp"
#include "FrameContext.hpp"
#include "ThreadContext.hpp"

#include <stdexcept>

namespace Renderer
{
    RenderContext::RenderContext()
    {
        mVulkanContext = std::make_unique<VulkanContext>();
        mEngineContext = std::make_unique<EngineContext>(*mVulkanContext);
        mFrameContext = std::make_unique<FrameContext>();
        mThreadContext = std::make_unique<ThreadContext>(*mVulkanContext);
    }

    RenderContext::~RenderContext()
    {
    }

    void RenderContext::UpdateContext(float dt)
    {
        mEngineContext->UpdateContext(*mVulkanContext, dt);
        mFrameContext->UpdateContext(mEngineContext->GetCamera());

    }

    void RenderContext::Cleanup()
    {
        mEngineContext->Cleanup(*mVulkanContext);
        mThreadContext->Cleanup(*mVulkanContext);
        mVulkanContext->Cleanup();
    }

    std::unique_ptr<VulkanContext>& RenderContext::GetVulkanContext()
    {
        return mVulkanContext;
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

    const std::unique_ptr<VulkanContext>& RenderContext::GetVulkanContext() const
    {
        return mVulkanContext;
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


}

