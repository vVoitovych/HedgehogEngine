#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <memory>

namespace HedgehogEngine
{
    class WindowContext;
    class EngineContext;
    class FrameContext;

    class Engine
    {
    public:
        HEDGEHOG_ENGINE_API Engine();
        HEDGEHOG_ENGINE_API ~Engine();

        Engine(const Engine&)            = delete;
        Engine& operator=(const Engine&) = delete;
        Engine(Engine&&)                 = delete;
        Engine& operator=(Engine&&)      = delete;

        // sceneAspect/gameAspect: aspect ratios of the scene and game viewports (they can differ);
        // the editor flycam uses sceneAspect, the ECS primary camera uses gameAspect.
        // tickGameLogic: when false (editor Edit/Pause), gameplay scripts are not ticked; the
        // scene still updates transforms/hierarchy/lights so editor edits reflect live.
        HEDGEHOG_ENGINE_API void UpdateContext(float dt, float sceneAspect, float gameAspect,
                                                bool tickGameLogic);
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
