#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "HedgehogEngine/HedgehogWindow/api/WindowDesc.hpp"

#include "Logger/api/Logger.hpp"

#include "GLFW/glfw3.h"

#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include "GLFW/glfw3native.h"
#endif

#include <cassert>
#include <cmath>
#include <functional>

namespace HW
{
    struct Window::Impl
    {
        GLFWwindow*           Handle       = nullptr;
        InputState            InputState;
        bool                  Resized      = false;
        bool                  IsFullscreen = false;
        int                   SavedX       = 0;
        int                   SavedY       = 0;
        int                   SavedWidth   = 1366;
        int                   SavedHeight  = 768;
        std::function<bool()> GuiCallback;
    };

    Window::Window(const WindowDesc& desc)
        : m_Impl(std::make_unique<Impl>())
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_Impl->IsFullscreen = desc.Fullscreen;
        m_Impl->SavedX       = desc.X;
        m_Impl->SavedY       = desc.Y;
        m_Impl->SavedWidth   = desc.Width;
        m_Impl->SavedHeight  = desc.Height;

        GLFWmonitor* monitor = desc.Fullscreen ? glfwGetPrimaryMonitor() : nullptr;
        m_Impl->Handle = glfwCreateWindow(desc.Width, desc.Height, desc.Title.c_str(), monitor, nullptr);
        assert(m_Impl->Handle != nullptr && "glfwCreateWindow() failed");

        if (!desc.Fullscreen)
            glfwSetWindowPos(m_Impl->Handle, desc.X, desc.Y);
        glfwSetWindowUserPointer(m_Impl->Handle, this);

        glfwSetFramebufferSizeCallback(m_Impl->Handle, OnFramebufferResize);
        glfwSetKeyCallback(m_Impl->Handle, OnKey);
        glfwSetMouseButtonCallback(m_Impl->Handle, OnMouseButton);
        glfwSetCursorPosCallback(m_Impl->Handle, OnMouseMove);
        glfwSetScrollCallback(m_Impl->Handle, OnMouseScroll);

        LOGINFO("Window created: ", desc.Title);
    }

    Window::~Window()
    {
        if (m_Impl->Handle)
        {
            glfwDestroyWindow(m_Impl->Handle);
            m_Impl->Handle = nullptr;
        }
        LOGINFO("Window destroyed");
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Impl->Handle) != 0;
    }

    bool Window::IsResized() const
    {
        return m_Impl->Resized;
    }

    void Window::ResetResizedFlag()
    {
        m_Impl->Resized = false;
    }

    void Window::WaitEvents() const
    {
        glfwWaitEvents();
    }

    void Window::ToggleFullscreen()
    {
        if (!m_Impl->IsFullscreen)
        {
            glfwGetWindowPos(m_Impl->Handle, &m_Impl->SavedX, &m_Impl->SavedY);
            glfwGetWindowSize(m_Impl->Handle, &m_Impl->SavedWidth, &m_Impl->SavedHeight);

            GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode    = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(m_Impl->Handle, monitor,
                                 0, 0, mode->width, mode->height, mode->refreshRate);
            m_Impl->IsFullscreen = true;
        }
        else
        {
            glfwSetWindowMonitor(m_Impl->Handle, nullptr,
                                 m_Impl->SavedX, m_Impl->SavedY,
                                 m_Impl->SavedWidth, m_Impl->SavedHeight, 0);
            m_Impl->IsFullscreen = false;
        }
    }

    bool Window::IsFullscreen() const
    {
        return m_Impl->IsFullscreen;
    }

    void Window::GetFramebufferSize(int& outWidth, int& outHeight) const
    {
        glfwGetFramebufferSize(m_Impl->Handle, &outWidth, &outHeight);
    }

    const InputState& Window::GetInputState() const
    {
        return m_Impl->InputState;
    }

    InputState& Window::GetInputState()
    {
        return m_Impl->InputState;
    }

    void Window::SetIcon(int width, int height, unsigned char* data)
    {
        GLFWimage image;
        image.width  = width;
        image.height = height;
        image.pixels = data;
        glfwSetWindowIcon(m_Impl->Handle, 1, &image);
    }

    void Window::SetGuiCallback(std::function<bool()> callback)
    {
        m_Impl->GuiCallback = std::move(callback);
    }

    GLFWwindow* Window::GetNativeHandle()
    {
        return m_Impl->Handle;
    }

    const GLFWwindow* Window::GetNativeHandle() const
    {
        return m_Impl->Handle;
    }

    void* Window::GetNativeOsHandle() const
    {
#ifdef _WIN32
        return static_cast<void*>(glfwGetWin32Window(m_Impl->Handle));
#else
        return nullptr;
#endif
    }

    const char** Window::GetVulkanExtensions(uint32_t& outCount) const
    {
        return glfwGetRequiredInstanceExtensions(&outCount);
    }

    void Window::OnFramebufferResize(GLFWwindow* handle, int /*width*/, int /*height*/)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        self->m_Impl->Resized = true;
    }

    void Window::OnKey(GLFWwindow* handle, int key, int /*scancode*/, int action, int mods)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        InputState& state = self->m_Impl->InputState;

        const bool pressOrRepeat = (action == GLFW_PRESS || action == GLFW_REPEAT);
        state.CtrlHeld = (mods & GLFW_MOD_CONTROL) != 0;

        switch (key)
        {
        case GLFW_KEY_W: state.KeyW = pressOrRepeat; break;
        case GLFW_KEY_S: state.KeyS = pressOrRepeat; break;
        case GLFW_KEY_A: state.KeyA = pressOrRepeat; break;
        case GLFW_KEY_D: state.KeyD = pressOrRepeat; break;
        case GLFW_KEY_Q: state.KeyQ = pressOrRepeat; break;
        case GLFW_KEY_E: state.KeyE = pressOrRepeat; break;
        case GLFW_KEY_F11:
            if (action == GLFW_PRESS)
                self->ToggleFullscreen();
            break;
        default: break;
        }
    }

    void Window::OnMouseButton(GLFWwindow* handle, int button, int action, int /*mods*/)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        if (self->m_Impl->GuiCallback && self->m_Impl->GuiCallback())
            return;

        InputState& state = self->m_Impl->InputState;
        const bool anyButtonDown = state.MouseLeft || state.MouseMiddle || state.MouseRight;

        if (action == GLFW_PRESS && !anyButtonDown)
        {
            double x = 0.0, y = 0.0;
            glfwGetCursorPos(handle, &x, &y);
            state.MousePos   = HM::Vector2(static_cast<float>(x), static_cast<float>(y));
            state.MouseDelta = HM::Vector2(0.0f, 0.0f);
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            state.MouseLeft = (action == GLFW_PRESS);
            if (action == GLFW_RELEASE)
                state.MouseDelta = HM::Vector2(0.0f, 0.0f);
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            state.MouseRight = (action == GLFW_PRESS);
            if (action == GLFW_RELEASE)
                state.MouseDelta = HM::Vector2(0.0f, 0.0f);
        }
        if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        {
            state.MouseMiddle = (action == GLFW_PRESS);
            if (action == GLFW_RELEASE)
                state.MouseDelta = HM::Vector2(0.0f, 0.0f);
        }
    }

    void Window::OnMouseMove(GLFWwindow* handle, double x, double y)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        InputState& state = self->m_Impl->InputState;

        if (state.MouseLeft || state.MouseMiddle || state.MouseRight)
        {
            const HM::Vector2 newPos(static_cast<float>(x), static_cast<float>(y));
            state.MouseDelta = newPos - state.MousePos;
            state.MousePos   = newPos;

            if (std::abs(state.MouseDelta.x()) < 2.0f)
                state.MouseDelta.x() = 0.0f;
            if (std::abs(state.MouseDelta.y()) < 2.0f)
                state.MouseDelta.y() = 0.0f;
        }
    }

    void Window::OnMouseScroll(GLFWwindow* handle, double x, double y)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        self->m_Impl->InputState.ScrollDelta = HM::Vector2(static_cast<float>(x), static_cast<float>(y));
    }
}
