#include "Context.hpp"

#include "VulkanContext.hpp"
#include "EngineContext.hpp"
#include "FrameContext.hpp"
#include "ThreadContext.hpp"

#include <stdexcept>

namespace Context
{
    Context::Context()
    {
        mVulkanContext = std::make_unique<VulkanContext>();
        mEngineContext = std::make_unique<EngineContext>(*mVulkanContext);
        mFrameContext = std::make_unique<FrameContext>();
        mThreadContext = std::make_unique<ThreadContext>(*mVulkanContext);
    }

    Context::~Context()
    {
    }

    void Context::UpdateContext(float dt)
    {
        mEngineContext->UpdateContext(*mVulkanContext, dt);
        mFrameContext->UpdateContext(mEngineContext->GetCamera());
        mThreadContext->Update(*mEngineContext, *mFrameContext);
    }

    void Context::Cleanup()
    {
        mEngineContext->Cleanup(*mVulkanContext);
        mThreadContext->Cleanup(*mVulkanContext);
        mVulkanContext->Cleanup();
    }

    VulkanContext& Context::GetVulkanContext()
    {
        return *mVulkanContext;
    }

    EngineContext& Context::GetEngineContext()
    {
        return *mEngineContext;
    }

    FrameContext& Context::GetFrameContext()
    {
        return *mFrameContext;
    }

    ThreadContext& Context::GetThreadContext()
    {
        return *mThreadContext;
    }

    const VulkanContext& Context::GetVulkanContext() const
    {
        return *mVulkanContext;
    }

    const EngineContext& Context::GetEngineContext() const
    {
        return *mEngineContext;
    }

    const FrameContext& Context::GetFrameContext() const
    {
        return *mFrameContext;
    }

    const ThreadContext& Context::GetThreadContext() const
    {
        return *mThreadContext;
    }


}

