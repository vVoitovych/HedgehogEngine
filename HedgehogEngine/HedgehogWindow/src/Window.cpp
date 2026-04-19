#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "HedgehogEngine/HedgehogWindow/api/WindowDesc.hpp"

#include "Logger/api/Logger.hpp"

#include "GLFW/glfw3.h"

#include <cmath>

namespace HW
{
    Window::Window(const WindowDesc& desc)
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        GLFWmonitor* monitor = desc.m_Fullscreen ? glfwGetPrimaryMonitor() : nullptr;
        m_Handle = glfwCreateWindow(desc.m_Width, desc.m_Height, desc.m_Title.c_str(), monitor, nullptr);

        glfwSetWindowPos(m_Handle, desc.m_X, desc.m_Y);
        glfwSetWindowUserPointer(m_Handle, this);

        glfwSetFramebufferSizeCallback(m_Handle, OnFramebufferResize);
        glfwSetKeyCallback(m_Handle, OnKey);
        glfwSetMouseButtonCallback(m_Handle, OnMouseButton);
        glfwSetCursorPosCallback(m_Handle, OnMouseMove);
        glfwSetScrollCallback(m_Handle, OnMouseScroll);

        LOGINFO("Window created: ", desc.m_Title);
    }

    Window::~Window()
    {
        if (m_Handle)
        {
            glfwDestroyWindow(m_Handle);
            m_Handle = nullptr;
        }
        LOGINFO("Window destroyed");
    }

    bool Window::ShouldClose() const
    {
        return glfwWindowShouldClose(m_Handle) != 0;
    }

    bool Window::IsResized() const
    {
        return m_Resized;
    }

    void Window::ResetResizedFlag()
    {
        m_Resized = false;
    }

    void Window::GetFramebufferSize(int& outWidth, int& outHeight) const
    {
        glfwGetFramebufferSize(m_Handle, &outWidth, &outHeight);
    }

    const InputState& Window::GetInputState() const
    {
        return m_InputState;
    }

    InputState& Window::GetInputState()
    {
        return m_InputState;
    }

    void Window::SetIcon(int width, int height, unsigned char* data)
    {
        GLFWimage image;
        image.width  = width;
        image.height = height;
        image.pixels = data;
        glfwSetWindowIcon(m_Handle, 1, &image);
    }

    void Window::SetGuiCallback(std::function<bool()> callback)
    {
        m_GuiCallback = std::move(callback);
    }

    GLFWwindow* Window::GetNativeHandle()
    {
        return m_Handle;
    }

    const GLFWwindow* Window::GetNativeHandle() const
    {
        return m_Handle;
    }

    void Window::OnFramebufferResize(GLFWwindow* handle, int /*width*/, int /*height*/)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        self->m_Resized = true;
    }

    void Window::OnKey(GLFWwindow* handle, int key, int /*scancode*/, int action, int mods)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        InputState& state = self->m_InputState;

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
        default: break;
        }
    }

    void Window::OnMouseButton(GLFWwindow* handle, int button, int action, int /*mods*/)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        if (self->m_GuiCallback && self->m_GuiCallback())
            return;

        InputState& state = self->m_InputState;
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
        InputState& state = self->m_InputState;

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
        self->m_InputState.m_ScrollDelta = HM::Vector2(static_cast<float>(x), static_cast<float>(y));
    }
}
