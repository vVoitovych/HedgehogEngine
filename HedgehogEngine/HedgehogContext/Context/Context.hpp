#pragma once

#include "HedgehogContext/api/HedgehogContextApi.hpp"

#include <memory>

namespace Context
{
    class WindowContext;
    class EngineContext;
    class FrameContext;

    class Context
    {
    public:
        HEDGEHOG_CONTEXT_API Context();
        HEDGEHOG_CONTEXT_API ~Context();

        Context(const Context&)            = delete;
        Context& operator=(const Context&) = delete;
        Context(Context&&)                 = delete;
        Context& operator=(Context&&)      = delete;

        HEDGEHOG_CONTEXT_API void UpdateContext(float dt, float aspectRatio);
        HEDGEHOG_CONTEXT_API void Cleanup();

        HEDGEHOG_CONTEXT_API WindowContext&       GetWindowContext();
        HEDGEHOG_CONTEXT_API const WindowContext& GetWindowContext() const;

        HEDGEHOG_CONTEXT_API EngineContext&       GetEngineContext();
        HEDGEHOG_CONTEXT_API const EngineContext& GetEngineContext() const;

        HEDGEHOG_CONTEXT_API FrameContext&        GetFrameContext();
        HEDGEHOG_CONTEXT_API const FrameContext&  GetFrameContext() const;

    private:
        std::unique_ptr<WindowContext> m_WindowContext;
        std::unique_ptr<EngineContext> m_EngineContext;
        std::unique_ptr<FrameContext>  m_FrameContext;
    };
}
