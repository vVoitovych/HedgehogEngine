#include "HedgehogEngine/api/WindowContext.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowDesc.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowManager.hpp"

#include "Logger/api/Logger.hpp"

namespace HedgehogEngine
{
    WindowContext::WindowContext()
    {
        m_WindowManager = std::make_unique<HW::WindowManager>();

        HW::WindowDesc desc;
        desc.Title  = "Hedgehog Engine";
        desc.X      = 100;
        desc.Y      = 100;
        desc.Width  = 1366;
        desc.Height = 768;
        m_Window = &m_WindowManager->CreateWindow(desc);
    }

    WindowContext::~WindowContext()
    {
    }

    void WindowContext::HandleInput()
    {
        m_WindowManager->PollEvents();
    }

    bool WindowContext::ShouldClose() const
    {
        return m_Window->ShouldClose();
    }

    HW::Window& WindowContext::GetWindow()
    {
        return *m_Window;
    }

    const HW::Window& WindowContext::GetWindow() const
    {
        return *m_Window;
    }
}
