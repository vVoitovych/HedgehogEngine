#pragma once

#include "HedgehogEngine/HedgehogWindow/api/HedgehogWindowApi.hpp"

#include <memory>

namespace HW
{
    class Window;
    struct WindowDesc;

    class WindowManager
    {
    public:
        HEDGEHOG_WINDOW_API WindowManager();
        HEDGEHOG_WINDOW_API ~WindowManager();

        WindowManager(const WindowManager&)            = delete;
        WindowManager& operator=(const WindowManager&) = delete;
        WindowManager(WindowManager&&)                 = delete;
        WindowManager& operator=(WindowManager&&)      = delete;

        HEDGEHOG_WINDOW_API Window& CreateWindow(const WindowDesc& desc);
        HEDGEHOG_WINDOW_API void    DestroyWindow(Window& window);

        HEDGEHOG_WINDOW_API void PollEvents() const;
        HEDGEHOG_WINDOW_API void WaitEvents() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
