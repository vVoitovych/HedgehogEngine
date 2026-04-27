#pragma once

#include "HedgehogContext/api/HedgehogContextApi.hpp"

#include <memory>

namespace HW
{
    class Window;
    class WindowManager;
}

namespace Context
{
    class WindowContext
    {
    public:
        HEDGEHOG_CONTEXT_API WindowContext();
        HEDGEHOG_CONTEXT_API ~WindowContext();

        WindowContext(const WindowContext&)            = delete;
        WindowContext(WindowContext&&)                 = delete;
        WindowContext& operator=(const WindowContext&) = delete;
        WindowContext& operator=(WindowContext&&)      = delete;

        HEDGEHOG_CONTEXT_API void HandleInput();
        HEDGEHOG_CONTEXT_API void WaitEvents();

        HEDGEHOG_CONTEXT_API bool ShouldClose() const;

        HEDGEHOG_CONTEXT_API void ResizeWindow();
        HEDGEHOG_CONTEXT_API bool IsWindowResized() const;
        HEDGEHOG_CONTEXT_API void ResetWindowResizeState();

        HEDGEHOG_CONTEXT_API HW::Window&       GetWindow();
        HEDGEHOG_CONTEXT_API const HW::Window& GetWindow() const;

    private:
        std::unique_ptr<HW::WindowManager> m_WindowManager;
        HW::Window*                        m_Window        = nullptr;
        bool                               m_WindowResized = false;
    };
}
