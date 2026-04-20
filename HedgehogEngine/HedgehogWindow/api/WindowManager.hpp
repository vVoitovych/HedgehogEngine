#pragma once

#include "HedgehogEngine/HedgehogWindow/api/HedgehogWindowApi.hpp"

namespace HW
{
    class Window;
    struct WindowDesc;

    class HEDGEHOG_WINDOW_API WindowManager
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
        struct Impl;
        Impl* m_Impl = nullptr;
    };
}
