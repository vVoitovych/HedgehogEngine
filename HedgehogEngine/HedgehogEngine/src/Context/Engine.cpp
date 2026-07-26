#include "HedgehogEngine/api/Engine.hpp"

#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/FrameContext.hpp"

namespace HedgehogEngine
{
    Engine::Engine()
    {
        m_WindowContext = std::make_unique<WindowContext>();
        m_EngineContext = std::make_unique<EngineContext>();
        m_FrameContext  = std::make_unique<FrameContext>();
    }

    Engine::~Engine()
    {
    }

    void Engine::UpdateContext(float dt, float sceneAspect, float gameAspect, bool tickGameLogic)
    {
        m_EngineContext->UpdateContext(*m_WindowContext, sceneAspect, gameAspect, dt, tickGameLogic);
        m_FrameContext->UpdateContext(m_EngineContext->GetCamera());
    }

    void Engine::Cleanup()
    {
    }

    WindowContext& Engine::GetWindowContext()
    {
        return *m_WindowContext;
    }

    const WindowContext& Engine::GetWindowContext() const
    {
        return *m_WindowContext;
    }

    EngineContext& Engine::GetEngineContext()
    {
        return *m_EngineContext;
    }

    const EngineContext& Engine::GetEngineContext() const
    {
        return *m_EngineContext;
    }

    FrameContext& Engine::GetFrameContext()
    {
        return *m_FrameContext;
    }

    const FrameContext& Engine::GetFrameContext() const
    {
        return *m_FrameContext;
    }
}
