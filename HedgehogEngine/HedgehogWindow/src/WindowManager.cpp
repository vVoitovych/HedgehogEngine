#include "HedgehogEngine/HedgehogWindow/api/WindowManager.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowDesc.hpp"

#include "Logger/api/Logger.hpp"

#include "GLFW/glfw3.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>

namespace HW
{
    struct WindowManager::Impl
    {
        std::vector<std::unique_ptr<Window>> m_Windows;
    };

    WindowManager::WindowManager()
        : m_Impl(new Impl())
    {
        const int result = glfwInit();
        assert(result == GLFW_TRUE && "glfwInit() failed");
        LOGINFO("Window manager initialized");
    }

    WindowManager::~WindowManager()
    {
        m_Impl->m_Windows.clear();
        delete m_Impl;
        m_Impl = nullptr;
        glfwTerminate();
        LOGINFO("Window manager cleaned");
    }

    Window& WindowManager::CreateWindow(const WindowDesc& desc)
    {
        m_Impl->m_Windows.emplace_back(std::make_unique<Window>(desc));
        return *m_Impl->m_Windows.back();
    }

    void WindowManager::DestroyWindow(Window& window)
    {
        auto& windows = m_Impl->m_Windows;
        auto it = std::find_if(windows.begin(), windows.end(),
            [&window](const std::unique_ptr<Window>& w) { return w.get() == &window; });

        assert(it != windows.end() && "Window not owned by this manager.");
        windows.erase(it);
    }

    void WindowManager::PollEvents() const
    {
        glfwPollEvents();
    }

    void WindowManager::WaitEvents() const
    {
        glfwWaitEvents();
    }
}
