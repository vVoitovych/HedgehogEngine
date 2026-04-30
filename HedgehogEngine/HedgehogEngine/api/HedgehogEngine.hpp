#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <memory>

namespace HedgehogEngine
{
    class WindowContext;
    class EngineContext;
    class FrameContext;

    class HedgehogEngine
    {
    public:
        HEDGEHOG_ENGINE_API HedgehogEngine();
        HEDGEHOG_ENGINE_API ~HedgehogEngine();

        HedgehogEngine(const HedgehogEngine&)            = delete;
        HedgehogEngine& operator=(const HedgehogEngine&) = delete;
        HedgehogEngine(HedgehogEngine&&)                 = delete;
        HedgehogEngine& operator=(HedgehogEngine&&)      = delete;

        HEDGEHOG_ENGINE_API void UpdateContext(float dt, float aspectRatio);
        HEDGEHOG_ENGINE_API void Cleanup();

        HEDGEHOG_ENGINE_API WindowContext&       GetWindowContext();
        HEDGEHOG_ENGINE_API const WindowContext& GetWindowContext() const;

        HEDGEHOG_ENGINE_API EngineContext&       GetEngineContext();
        HEDGEHOG_ENGINE_API const EngineContext& GetEngineContext() const;

        HEDGEHOG_ENGINE_API FrameContext&        GetFrameContext();
        HEDGEHOG_ENGINE_API const FrameContext&  GetFrameContext() const;

    private:
        std::unique_ptr<WindowContext> m_WindowContext;
        std::unique_ptr<EngineContext> m_EngineContext;
        std::unique_ptr<FrameContext>  m_FrameContext;
    };
}
