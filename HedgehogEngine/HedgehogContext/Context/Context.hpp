#pragma once

#include <memory>

namespace Context
{
    class WindowContext;
    class EngineContext;
    class FrameContext;

    class Context
    {
    public:
        Context();
        ~Context();

        Context(const Context&)            = delete;
        Context& operator=(const Context&) = delete;
        Context(Context&&)                 = delete;
        Context& operator=(Context&&)      = delete;

        void UpdateContext(float dt, float aspectRatio);
        void Cleanup();

        WindowContext&       GetWindowContext();
        const WindowContext& GetWindowContext() const;

        EngineContext&       GetEngineContext();
        const EngineContext& GetEngineContext() const;

        FrameContext&        GetFrameContext();
        const FrameContext&  GetFrameContext() const;

    private:
        std::unique_ptr<WindowContext> m_WindowContext;
        std::unique_ptr<EngineContext> m_EngineContext;
        std::unique_ptr<FrameContext>  m_FrameContext;
    };
}
