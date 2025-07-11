#include "Context.hpp"

#include "VulkanContext.hpp"
#include "EngineContext.hpp"
#include "RendererContext.hpp"

#include <stdexcept>

namespace Context
{
    Context::Context()
    {
        m_VulkanContext = std::make_unique<VulkanContext>();
        m_EngineContext = std::make_unique<EngineContext>(*m_VulkanContext);
        m_RendererContext = std::make_unique<RendererContext>(*m_VulkanContext);
    }

    Context::~Context()
    {
    }

    void Context::UpdateContext(float dt)
    {
        m_EngineContext->UpdateContext(*m_VulkanContext, dt);
    }

    void Context::Cleanup()
    {
        m_EngineContext->Cleanup(*m_VulkanContext);
        m_RendererContext->Cleanup(*m_VulkanContext);
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


    RendererContext& Context::GetRendererContext()
    {
        return *m_RendererContext;
    }

    const VulkanContext& Context::GetVulkanContext() const
    {
        return *m_VulkanContext;
    }

    const EngineContext& Context::GetEngineContext() const
    {
        return *m_EngineContext;
    }

    const RendererContext& Context::GetRendererContext() const
    {
        return *m_RendererContext;
    }


}

