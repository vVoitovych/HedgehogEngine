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
        m_VulkanContext = std::make_unique<VulkanContext>();
        m_EngineContext = std::make_unique<EngineContext>(*m_VulkanContext);
        m_FrameContext = std::make_unique<FrameContext>();
        m_ThreadContext = std::make_unique<ThreadContext>(*m_VulkanContext);
    }

    Context::~Context()
    {
    }

    void Context::UpdateContext(float dt)
    {
        m_EngineContext->UpdateContext(*m_VulkanContext, dt);
        m_FrameContext->UpdateContext(m_EngineContext->GetCamera());
    }

    void Context::Cleanup()
    {
        m_EngineContext->Cleanup(*m_VulkanContext);
        m_ThreadContext->Cleanup(*m_VulkanContext);
        m_VulkanContext->Cleanup();
    }

    VulkanContext& Context::GetVulkanContext()
    {
        return *m_VulkanContext;
    }

    EngineContext& Context::GetEngineContext()
    {
        return *m_EngineContext;
    }

    FrameContext& Context::GetFrameContext()
    {
        return *m_FrameContext;
    }

    ThreadContext& Context::GetThreadContext()
    {
        return *m_ThreadContext;
    }

    const VulkanContext& Context::GetVulkanContext() const
    {
        return *m_VulkanContext;
    }

    const EngineContext& Context::GetEngineContext() const
    {
        return *m_EngineContext;
    }

    const FrameContext& Context::GetFrameContext() const
    {
        return *m_FrameContext;
    }

    const ThreadContext& Context::GetThreadContext() const
    {
        return *m_ThreadContext;
    }


}

