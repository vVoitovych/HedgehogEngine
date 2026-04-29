#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <memory>

namespace HW
{
    class Window;
    class WindowManager;
}

namespace HedgehogEngine
{
    class WindowContext
    {
    public:
        HEDGEHOG_ENGINE_API WindowContext();
        HEDGEHOG_ENGINE_API ~WindowContext();

        WindowContext(const WindowContext&)            = delete;
        WindowContext(WindowContext&&)                 = delete;
        WindowContext& operator=(const WindowContext&) = delete;
        WindowContext& operator=(WindowContext&&)      = delete;

        HEDGEHOG_ENGINE_API void HandleInput();
        HEDGEHOG_ENGINE_API void WaitEvents();

        HEDGEHOG_ENGINE_API bool ShouldClose() const;

        HEDGEHOG_ENGINE_API void ResizeWindow();
        HEDGEHOG_ENGINE_API bool IsWindowResized() const;
        HEDGEHOG_ENGINE_API void ResetWindowResizeState();

        HEDGEHOG_ENGINE_API HW::Window&       GetWindow();
        HEDGEHOG_ENGINE_API const HW::Window& GetWindow() const;

    private:
        std::unique_ptr<HW::WindowManager> m_WindowManager;
        HW::Window*                        m_Window        = nullptr;
        bool                               m_WindowResized = false;
    };
}
