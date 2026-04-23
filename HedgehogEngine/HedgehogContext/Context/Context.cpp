#include "Context.hpp"

#include "WindowContext.hpp"
#include "EngineContext.hpp"
#include "FrameContext.hpp"

namespace Context
{
    Context::Context()
    {
        m_WindowContext = std::make_unique<WindowContext>();
        m_EngineContext = std::make_unique<EngineContext>();
        m_FrameContext  = std::make_unique<FrameContext>();
    }

    Context::~Context()
    {
    }

    void Context::UpdateContext(float dt, float aspectRatio)
    {
        m_EngineContext->UpdateContext(*m_WindowContext, aspectRatio, dt);
        m_FrameContext->UpdateContext(m_EngineContext->GetCamera());
    }

    void Context::Cleanup()
    {
    }

    WindowContext& Context::GetWindowContext()
    {
        return *m_WindowContext;
    }

    const WindowContext& Context::GetWindowContext() const
    {
        return *m_WindowContext;
    }

    EngineContext& Context::GetEngineContext()
    {
        return *m_EngineContext;
    }

    const EngineContext& Context::GetEngineContext() const
    {
        return *m_EngineContext;
    }

    FrameContext& Context::GetFrameContext()
    {
        return *m_FrameContext;
    }

    const FrameContext& Context::GetFrameContext() const
    {
        return *m_FrameContext;
    }
}
