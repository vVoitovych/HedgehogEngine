#pragma once

#include <memory>
#include <vector>

namespace HW
{
    class Window;
    struct WindowDesc;

    class WindowManager
    {
    public:
        WindowManager();
        ~WindowManager();

        WindowManager(const WindowManager&)            = delete;
        WindowManager& operator=(const WindowManager&) = delete;
        WindowManager(WindowManager&&)                 = delete;
        WindowManager& operator=(WindowManager&&)      = delete;

        Window& CreateWindow(const WindowDesc& desc);
        void    DestroyWindow(Window& window);

        void PollEvents() const;
        void WaitEvents() const;

    private:
        std::vector<std::unique_ptr<Window>> m_Windows;
    };
}
