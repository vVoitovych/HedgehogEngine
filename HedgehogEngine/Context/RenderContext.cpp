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
        mThreadContext->Update(*mEngineContext, *mFrameContext);
    }

    void RenderContext::Cleanup()
    {
        mEngineContext->Cleanup(*mVulkanContext);
        mThreadContext->Cleanup(*mVulkanContext);
        mVulkanContext->Cleanup();
    }

    VulkanContext& RenderContext::GetVulkanContext()
    {
        return *mVulkanContext;
    }

    EngineContext& RenderContext::GetEngineContext()
    {
        return *mEngineContext;
    }

    FrameContext& RenderContext::GetFrameContext()
    {
        return *mFrameContext;
    }

    ThreadContext& RenderContext::GetThreadContext()
    {
        return *mThreadContext;
    }

    const VulkanContext& RenderContext::GetVulkanContext() const
    {
        return *mVulkanContext;
    }

    const EngineContext& RenderContext::GetEngineContext() const
    {
        return *mEngineContext;
    }

    const FrameContext& RenderContext::GetFrameContext() const
    {
        return *mFrameContext;
    }

    const ThreadContext& RenderContext::GetThreadContext() const
    {
        return *mThreadContext;
    }


}

