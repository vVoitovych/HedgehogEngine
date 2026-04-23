#pragma once

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
        WindowContext();
        ~WindowContext();

        WindowContext(const WindowContext&)            = delete;
        WindowContext(WindowContext&&)                 = delete;
        WindowContext& operator=(const WindowContext&) = delete;
        WindowContext& operator=(WindowContext&&)      = delete;

        void HandleInput();
        void WaitEvents();

        bool ShouldClose() const;

        void ResizeWindow();
        bool IsWindowResized() const;
        void ResetWindowResizeState();

        HW::Window&       GetWindow();
        const HW::Window& GetWindow() const;

    private:
        std::unique_ptr<HW::WindowManager> m_WindowManager;
        HW::Window*                        m_Window        = nullptr;
        bool                               m_WindowResized = false;
    };
}
