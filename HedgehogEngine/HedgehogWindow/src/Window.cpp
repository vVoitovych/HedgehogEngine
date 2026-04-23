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
        GLFWwindow*           m_Handle       = nullptr;
        InputState            m_InputState;
        bool                  m_Resized      = false;
        bool                  m_IsFullscreen = false;
        int                   m_SavedX       = 0;
        int                   m_SavedY       = 0;
        int                   m_SavedWidth   = 1366;
        int                   m_SavedHeight  = 768;
        std::function<bool()> m_GuiCallback;
    };

    Window::Window(const WindowDesc& desc)
        : m_Impl(new Impl())
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        m_Impl->m_IsFullscreen = desc.m_Fullscreen;
        m_Impl->m_SavedX       = desc.m_X;
        m_Impl->m_SavedY       = desc.m_Y;
        m_Impl->m_SavedWidth   = desc.m_Width;
        m_Impl->m_SavedHeight  = desc.m_Height;

        GLFWmonitor* monitor = desc.m_Fullscreen ? glfwGetPrimaryMonitor() : nullptr;
        m_Impl->m_Handle = glfwCreateWindow(desc.m_Width, desc.m_Height, desc.m_Title.c_str(), monitor, nullptr);
        assert(m_Impl->m_Handle != nullptr && "glfwCreateWindow() failed");

        if (!desc.m_Fullscreen)
            glfwSetWindowPos(m_Impl->m_Handle, desc.m_X, desc.m_Y);
        glfwSetWindowUserPointer(m_Impl->m_Handle, this);

        glfwSetFramebufferSizeCallback(m_Impl->m_Handle, OnFramebufferResize);
        glfwSetKeyCallback(m_Impl->m_Handle, OnKey);
        glfwSetMouseButtonCallback(m_Impl->m_Handle, OnMouseButton);
        glfwSetCursorPosCallback(m_Impl->m_Handle, OnMouseMove);
        glfwSetScrollCallback(m_Impl->m_Handle, OnMouseScroll);

        LOGINFO("Window created: ", desc.m_Title);
    }

    Window::~Window()
    {
        if (m_Impl->m_Handle)
        {
            glfwDestroyWindow(m_Impl->m_Handle);
            m_Impl->m_Handle = nullptr;
        }
        delete m_Impl;
        m_Impl = nullptr;
        LOGINFO("Window destroyed");
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Impl->m_Handle) != 0;
    }

    bool Window::IsResized() const
    {
        return m_Impl->m_Resized;
    }

    void Window::ResetResizedFlag()
    {
        m_Impl->m_Resized = false;
    }

    void Window::ToggleFullscreen()
    {
        if (!m_Impl->m_IsFullscreen)
        {
            glfwGetWindowPos(m_Impl->m_Handle, &m_Impl->m_SavedX, &m_Impl->m_SavedY);
            glfwGetWindowSize(m_Impl->m_Handle, &m_Impl->m_SavedWidth, &m_Impl->m_SavedHeight);

            GLFWmonitor*       monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode    = glfwGetVideoMode(monitor);
            glfwSetWindowMonitor(m_Impl->m_Handle, monitor,
                                 0, 0, mode->width, mode->height, mode->refreshRate);
            m_Impl->m_IsFullscreen = true;
        }
        else
        {
            glfwSetWindowMonitor(m_Impl->m_Handle, nullptr,
                                 m_Impl->m_SavedX, m_Impl->m_SavedY,
                                 m_Impl->m_SavedWidth, m_Impl->m_SavedHeight, 0);
            m_Impl->m_IsFullscreen = false;
        }
    }

    bool Window::IsFullscreen() const
    {
        return m_Impl->m_IsFullscreen;
    }

    void Window::GetFramebufferSize(int& outWidth, int& outHeight) const
    {
        glfwGetFramebufferSize(m_Impl->m_Handle, &outWidth, &outHeight);
    }

    const InputState& Window::GetInputState() const
    {
        return m_Impl->m_InputState;
    }

    InputState& Window::GetInputState()
    {
        return m_Impl->m_InputState;
    }

    void Window::SetIcon(int width, int height, unsigned char* data)
    {
        GLFWimage image;
        image.width  = width;
        image.height = height;
        image.pixels = data;
        glfwSetWindowIcon(m_Impl->m_Handle, 1, &image);
    }

    void Window::SetGuiCallback(std::function<bool()> callback)
    {
        m_Impl->m_GuiCallback = std::move(callback);
    }

    GLFWwindow* Window::GetNativeHandle()
    {
        return m_Impl->m_Handle;
    }

    const GLFWwindow* Window::GetNativeHandle() const
    {
        return m_Impl->m_Handle;
    }

    void* Window::GetNativeOsHandle() const
    {
#ifdef _WIN32
        return static_cast<void*>(glfwGetWin32Window(m_Impl->m_Handle));
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
        self->m_Impl->m_Resized = true;
    }

    void Window::OnKey(GLFWwindow* handle, int key, int /*scancode*/, int action, int mods)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        InputState& state = self->m_Impl->m_InputState;

        const bool pressOrRepeat = (action == GLFW_PRESS || action == GLFW_REPEAT);
        state.m_CtrlHeld = (mods & GLFW_MOD_CONTROL) != 0;

        switch (key)
        {
        case GLFW_KEY_W: state.m_KeyW = pressOrRepeat; break;
        case GLFW_KEY_S: state.m_KeyS = pressOrRepeat; break;
        case GLFW_KEY_A: state.m_KeyA = pressOrRepeat; break;
        case GLFW_KEY_D: state.m_KeyD = pressOrRepeat; break;
        case GLFW_KEY_Q: state.m_KeyQ = pressOrRepeat; break;
        case GLFW_KEY_E: state.m_KeyE = pressOrRepeat; break;
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
        if (self->m_Impl->m_GuiCallback && self->m_Impl->m_GuiCallback())
            return;

        InputState& state = self->m_Impl->m_InputState;
        const bool anyButtonDown = state.m_MouseLeft || state.m_MouseMiddle || state.m_MouseRight;

        if (action == GLFW_PRESS && !anyButtonDown)
        {
            double x = 0.0, y = 0.0;
            glfwGetCursorPos(handle, &x, &y);
            state.m_MousePos   = HM::Vector2(static_cast<float>(x), static_cast<float>(y));
            state.m_MouseDelta = HM::Vector2(0.0f, 0.0f);
        }

        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            state.m_MouseLeft = (action == GLFW_PRESS);
            if (action == GLFW_RELEASE)
                state.m_MouseDelta = HM::Vector2(0.0f, 0.0f);
        }
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            state.m_MouseRight = (action == GLFW_PRESS);
            if (action == GLFW_RELEASE)
                state.m_MouseDelta = HM::Vector2(0.0f, 0.0f);
        }
        if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        {
            state.m_MouseMiddle = (action == GLFW_PRESS);
            if (action == GLFW_RELEASE)
                state.m_MouseDelta = HM::Vector2(0.0f, 0.0f);
        }
    }

    void Window::OnMouseMove(GLFWwindow* handle, double x, double y)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        InputState& state = self->m_Impl->m_InputState;

        if (state.m_MouseLeft || state.m_MouseMiddle || state.m_MouseRight)
        {
            const HM::Vector2 newPos(static_cast<float>(x), static_cast<float>(y));
            state.m_MouseDelta = newPos - state.m_MousePos;
            state.m_MousePos   = newPos;

            if (std::abs(state.m_MouseDelta.x()) < 2.0f)
                state.m_MouseDelta.x() = 0.0f;
            if (std::abs(state.m_MouseDelta.y()) < 2.0f)
                state.m_MouseDelta.y() = 0.0f;
        }
    }

    void Window::OnMouseScroll(GLFWwindow* handle, double x, double y)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        self->m_Impl->m_InputState.m_ScrollDelta = HM::Vector2(static_cast<float>(x), static_cast<float>(y));
    }
}
