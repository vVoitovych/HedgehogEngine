#include "HedgehogEngine.hpp"

#include "WindowContext.hpp"
#include "EngineContext.hpp"
#include "FrameContext.hpp"

namespace HedgehogEngine
{
    HedgehogEngine::HedgehogEngine()
    {
        m_WindowContext = std::make_unique<WindowContext>();
        m_EngineContext = std::make_unique<EngineContext>();
        m_FrameContext  = std::make_unique<FrameContext>();
    }

    HedgehogEngine::~HedgehogEngine()
    {
    }

    void HedgehogEngine::UpdateContext(float dt, float aspectRatio)
    {
        m_EngineContext->UpdateContext(*m_WindowContext, aspectRatio, dt);
        m_FrameContext->UpdateContext(m_EngineContext->GetCamera());
    }

    void HedgehogEngine::Cleanup()
    {
    }

    WindowContext& HedgehogEngine::GetWindowContext()
    {
        return *m_WindowContext;
    }

    const WindowContext& HedgehogEngine::GetWindowContext() const
    {
        return *m_WindowContext;
    }

    EngineContext& HedgehogEngine::GetEngineContext()
    {
        return *m_EngineContext;
    }

    const EngineContext& HedgehogEngine::GetEngineContext() const
    {
        return *m_EngineContext;
    }

    FrameContext& HedgehogEngine::GetFrameContext()
    {
        return *m_FrameContext;
    }

    const FrameContext& HedgehogEngine::GetFrameContext() const
    {
        return *m_FrameContext;
    }
}
