#include "HedgehogEngine/HedgehogWindow/api/WindowManager.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowDesc.hpp"

#include "Logger/api/Logger.hpp"

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <algorithm>
#include <cassert>

namespace HW
{
    WindowManager::WindowManager()
    {
        glfwInit();
        LOGINFO("Window manager initialized");
    }

    WindowManager::~WindowManager()
    {
        m_Windows.clear();
        glfwTerminate();
        LOGINFO("Window manager cleaned");
    }

    Window& WindowManager::CreateWindow(const WindowDesc& desc)
    {
        m_Windows.emplace_back(std::make_unique<Window>(desc));
        return *m_Windows.back();
    }

    void WindowManager::DestroyWindow(Window& window)
    {
        auto it = std::find_if(m_Windows.begin(), m_Windows.end(),
            [&window](const std::unique_ptr<Window>& w) { return w.get() == &window; });

        assert(it != m_Windows.end() && "Window not owned by this manager.");
        m_Windows.erase(it);
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
