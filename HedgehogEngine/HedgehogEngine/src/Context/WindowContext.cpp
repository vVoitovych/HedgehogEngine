#include "HedgehogEngine/api/WindowContext.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowDesc.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowManager.hpp"

#include "Logger/api/Logger.hpp"

namespace HedgehogEngine
{
    WindowContext::WindowContext(WindowMode mode)
        : WindowContext(WindowOptions{ .Mode = mode })
    {
    }

    WindowContext::WindowContext(const WindowOptions& options)
    {
        m_WindowManager = std::make_unique<HW::WindowManager>();

        HW::WindowDesc desc;
        desc.Title      = options.Title;
        desc.X          = 100;
        desc.Y          = 100;
        desc.Width      = options.Width;
        desc.Height     = options.Height;
        desc.Fullscreen = options.Fullscreen;
        desc.Maximized  = options.Mode == WindowMode::Maximized;
        m_Window = &m_WindowManager->CreateWindow(desc);
    }

    WindowContext::~WindowContext()
    {
    }

    void WindowContext::HandleInput()
    {
        // The raw input's deltas sum this frame's events.
        m_Window->BeginInputFrame();
        m_WindowManager->PollEvents();
        m_Window->PollGamepad();
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
