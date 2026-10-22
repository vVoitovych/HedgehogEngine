#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "HedgehogEngine/HedgehogWindow/api/RawInputEvents.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowDesc.hpp"

#include "Logger/api/Logger.hpp"

#include "GLFW/glfw3.h"

#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include "GLFW/glfw3native.h"
#endif

#include <array>
#include <cassert>
#include <vector>

namespace HW
{
    // InputCodes.hpp's values are GLFW's: the first and last code of every run of keys.
    static_assert(static_cast<int>(Key::Space) == GLFW_KEY_SPACE);
    static_assert(static_cast<int>(Key::Apostrophe) == GLFW_KEY_APOSTROPHE);
    static_assert(static_cast<int>(Key::Comma) == GLFW_KEY_COMMA && static_cast<int>(Key::Num9) == GLFW_KEY_9);
    static_assert(static_cast<int>(Key::Semicolon) == GLFW_KEY_SEMICOLON);
    static_assert(static_cast<int>(Key::Equal) == GLFW_KEY_EQUAL);
    static_assert(static_cast<int>(Key::A) == GLFW_KEY_A && static_cast<int>(Key::RightBracket) == GLFW_KEY_RIGHT_BRACKET);
    static_assert(static_cast<int>(Key::GraveAccent) == GLFW_KEY_GRAVE_ACCENT);
    static_assert(static_cast<int>(Key::World1) == GLFW_KEY_WORLD_1 && static_cast<int>(Key::World2) == GLFW_KEY_WORLD_2);
    static_assert(static_cast<int>(Key::Escape) == GLFW_KEY_ESCAPE && static_cast<int>(Key::End) == GLFW_KEY_END);
    static_assert(static_cast<int>(Key::Right) == GLFW_KEY_RIGHT && static_cast<int>(Key::Up) == GLFW_KEY_UP);
    static_assert(static_cast<int>(Key::CapsLock) == GLFW_KEY_CAPS_LOCK && static_cast<int>(Key::Pause) == GLFW_KEY_PAUSE);
    static_assert(static_cast<int>(Key::F1) == GLFW_KEY_F1 && static_cast<int>(Key::F25) == GLFW_KEY_F25);
    static_assert(static_cast<int>(Key::Keypad0) == GLFW_KEY_KP_0 && static_cast<int>(Key::KeypadEqual) == GLFW_KEY_KP_EQUAL);
    static_assert(static_cast<int>(Key::KeypadEnter) == GLFW_KEY_KP_ENTER);
    static_assert(static_cast<int>(Key::LeftShift) == GLFW_KEY_LEFT_SHIFT && static_cast<int>(Key::Menu) == GLFW_KEY_MENU);
    static_assert(KEY_COUNT == GLFW_KEY_LAST + 1);
    static_assert(static_cast<int>(MouseButton::Left) == GLFW_MOUSE_BUTTON_LEFT);
    static_assert(static_cast<int>(MouseButton::Right) == GLFW_MOUSE_BUTTON_RIGHT);
    static_assert(static_cast<int>(MouseButton::Middle) == GLFW_MOUSE_BUTTON_MIDDLE);
    static_assert(MOUSE_BUTTON_COUNT == GLFW_MOUSE_BUTTON_LAST + 1);
    static_assert(static_cast<int>(GamepadButton::A) == GLFW_GAMEPAD_BUTTON_A);
    static_assert(static_cast<int>(GamepadButton::Guide) == GLFW_GAMEPAD_BUTTON_GUIDE);
    static_assert(static_cast<int>(GamepadButton::DpadLeft) == GLFW_GAMEPAD_BUTTON_DPAD_LEFT);
    static_assert(GAMEPAD_BUTTON_COUNT == GLFW_GAMEPAD_BUTTON_LAST + 1);
    static_assert(static_cast<int>(GamepadAxis::LeftY) == GLFW_GAMEPAD_AXIS_LEFT_Y);
    static_assert(static_cast<int>(GamepadAxis::RightTrigger) == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER);
    static_assert(GAMEPAD_AXIS_COUNT == GLFW_GAMEPAD_AXIS_LAST + 1);

    // What a window drawing its own title bar keeps for its window procedure.
    struct CustomTitleBar
    {
        std::vector<ClientRect> CaptionAreas; // what drags the window
#ifdef _WIN32
        LONG_PTR                GlfwProc = 0; // GLFW's window procedure, which ours forwards to
#endif
    };

    struct Window::Impl
    {
        GLFWwindow*           Handle       = nullptr;
        std::string           Title;
        RawInput              Raw;
        bool                  Resized      = false;
        bool                  IsFullscreen = false;
        int                   SavedX       = 0;
        int                   SavedY       = 0;
        int                   SavedWidth   = 1366;
        int                   SavedHeight  = 768;
        CustomTitleBar        TitleBar;
    };

#ifdef _WIN32
    namespace
    {
        constexpr const wchar_t* WINDOW_PROPERTY = L"HedgehogWindow";

        // The resize frame Windows keeps around a window with WS_THICKFRAME (mostly invisible).
        int FrameThickness(HWND hwnd, bool vertical)
        {
            const UINT dpi = GetDpiForWindow(hwnd);
            return GetSystemMetricsForDpi(vertical ? SM_CYFRAME : SM_CXFRAME, dpi) +
                   GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        }

        LRESULT CALLBACK TitleBarProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            auto* bar      = static_cast<CustomTitleBar*>(GetPropW(hwnd, WINDOW_PROPERTY));
            auto  glfwProc = reinterpret_cast<WNDPROC>(bar->GlfwProc);
            switch (message)
            {
            case WM_NCCALCSIZE:
                if (wParam == TRUE)
                {
                    // The default frame on the sides and bottom, none on top: the client area takes
                    // the caption's place. A maximized window reaches past its monitor by the frame,
                    // so its top is pushed back in.
                    auto&      rect = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam)->rgrc[0];
                    const LONG top  = rect.top;
                    const LRESULT result = DefWindowProcW(hwnd, message, wParam, lParam);
                    rect.top = top + (IsZoomed(hwnd) ? FrameThickness(hwnd, true) : 0);
                    return result;
                }
                break;
            case WM_NCHITTEST:
            {
                const LRESULT hit = DefWindowProcW(hwnd, message, wParam, lParam);
                if (hit != HTCLIENT)
                    return hit; // the side and bottom edges, which resize
                POINT point = { static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)) };
                ScreenToClient(hwnd, &point);
                switch (HitTestTitleBar(point.x, point.y, IsZoomed(hwnd) != 0, FrameThickness(hwnd, true), bar->CaptionAreas))
                {
                case TitleBarHit::Top:     return HTTOP;
                case TitleBarHit::Caption: return HTCAPTION;
                case TitleBarHit::Client:  return HTCLIENT;
                }
                return HTCLIENT;
            }
            default:
                break;
            }
            return CallWindowProcW(glfwProc, hwnd, message, wParam, lParam);
        }
    }
#endif

    Window::Window(const WindowDesc& desc)
        : m_Impl(std::make_unique<Impl>())
    {
        // Hints persist across glfwCreateWindow calls, so start from the defaults for every window.
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // The initial position is a hint rather than a glfwSetWindowPos after creation, which
        // would move a maximized window out of its maximized state.
        glfwWindowHint(GLFW_POSITION_X, desc.X);
        glfwWindowHint(GLFW_POSITION_Y, desc.Y);
        glfwWindowHint(GLFW_MAXIMIZED, desc.Maximized ? GLFW_TRUE : GLFW_FALSE);

        m_Impl->IsFullscreen = desc.Fullscreen;
        m_Impl->SavedX       = desc.X;
        m_Impl->SavedY       = desc.Y;
        m_Impl->SavedWidth   = desc.Width;
        m_Impl->SavedHeight  = desc.Height;

        GLFWmonitor* monitor = desc.Fullscreen ? glfwGetPrimaryMonitor() : nullptr;
        m_Impl->Handle = glfwCreateWindow(desc.Width, desc.Height, desc.Title.c_str(), monitor, nullptr);
        m_Impl->Title  = desc.Title;
        assert(m_Impl->Handle != nullptr && "glfwCreateWindow() failed");

        glfwSetWindowUserPointer(m_Impl->Handle, this);

        glfwSetFramebufferSizeCallback(m_Impl->Handle, OnFramebufferResize);
        glfwSetKeyCallback(m_Impl->Handle, OnKey);
        glfwSetMouseButtonCallback(m_Impl->Handle, OnMouseButton);
        glfwSetCursorPosCallback(m_Impl->Handle, OnMouseMove);
        glfwSetScrollCallback(m_Impl->Handle, OnMouseScroll);
        glfwSetWindowFocusCallback(m_Impl->Handle, OnFocus);
        glfwSetCursorEnterCallback(m_Impl->Handle, OnCursorEnter);
        m_Impl->Raw.Focused = glfwGetWindowAttrib(m_Impl->Handle, GLFW_FOCUSED) != 0;

        LOGINFO("Window created: ", desc.Title);
    }

    Window::~Window()
    {
        if (m_Impl->Handle)
        {
#ifdef _WIN32
            if (m_Impl->TitleBar.GlfwProc != 0)
            {
                const HWND hwnd = glfwGetWin32Window(m_Impl->Handle);
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, m_Impl->TitleBar.GlfwProc);
                RemovePropW(hwnd, WINDOW_PROPERTY);
            }
#endif
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

    void Window::PollEvents() const
    {
        glfwPollEvents();
    }

    const std::string& Window::GetTitle() const { return m_Impl->Title; }

    void Window::SetTitle(const std::string& title)
    {
        m_Impl->Title = title;
        glfwSetWindowTitle(m_Impl->Handle, title.c_str());
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

    void Window::GetWindowSize(int& outWidth, int& outHeight) const
    {
        glfwGetWindowSize(m_Impl->Handle, &outWidth, &outHeight);
    }

    void Window::SetIcon(int width, int height, unsigned char* data)
    {
        GLFWimage image;
        image.width  = width;
        image.height = height;
        image.pixels = data;
        glfwSetWindowIcon(m_Impl->Handle, 1, &image);
    }

    void Window::UseCustomTitleBar()
    {
#ifdef _WIN32
        if (m_Impl->TitleBar.GlfwProc != 0)
            return;
        const HWND hwnd = glfwGetWin32Window(m_Impl->Handle);
        SetPropW(hwnd, WINDOW_PROPERTY, &m_Impl->TitleBar);
        m_Impl->TitleBar.GlfwProc = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&TitleBarProc));
        // Has Windows ask the new procedure for the frame.
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
#endif
    }

    void Window::SetCaptionAreas(std::span<const ClientRect> areas)
    {
        m_Impl->TitleBar.CaptionAreas.assign(areas.begin(), areas.end());
    }

    void Window::Minimize()
    {
        glfwIconifyWindow(m_Impl->Handle);
    }

    void Window::ToggleMaximize()
    {
        if (IsMaximized())
            glfwRestoreWindow(m_Impl->Handle);
        else
            glfwMaximizeWindow(m_Impl->Handle);
    }

    bool Window::IsMaximized() const
    {
        return glfwGetWindowAttrib(m_Impl->Handle, GLFW_MAXIMIZED) != 0;
    }

    void Window::RequestClose()
    {
        glfwSetWindowShouldClose(m_Impl->Handle, GLFW_TRUE);
    }

    GLFWwindow* Window::GetNativeHandle()
    {
        return m_Impl->Handle;
    }

    const GLFWwindow* Window::GetNativeHandle() const
    {
        return m_Impl->Handle;
    }

    const RawInput& Window::GetRawInput() const
    {
        return m_Impl->Raw;
    }

    RawInput& Window::GetRawInput()
    {
        return m_Impl->Raw;
    }

    void Window::BeginInputFrame()
    {
        HW::BeginInputFrame(m_Impl->Raw);
    }

    void Window::PollGamepad()
    {
        for (int joystick = GLFW_JOYSTICK_1; joystick <= GLFW_JOYSTICK_LAST; ++joystick)
        {
            GLFWgamepadstate state;
            if (!glfwJoystickIsGamepad(joystick) || !glfwGetGamepadState(joystick, &state))
                continue;
            std::array<bool, GAMEPAD_BUTTON_COUNT> buttons{};
            for (size_t i = 0; i < GAMEPAD_BUTTON_COUNT; ++i)
                buttons[i] = state.buttons[i] == GLFW_PRESS;
            ApplyGamepadState(m_Impl->Raw, true, buttons, state.axes);
            return;
        }
        ApplyGamepadState(m_Impl->Raw, false, {}, {});
    }

    void* Window::GetNativeOsHandle() const
    {
#ifdef _WIN32
        return static_cast<void*>(glfwGetWin32Window(m_Impl->Handle));
#else
        return nullptr;
#endif
    }

    void Window::OnFramebufferResize(GLFWwindow* handle, int /*width*/, int /*height*/)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        self->m_Impl->Resized = true;
    }

    void Window::OnKey(GLFWwindow* handle, int key, int /*scancode*/, int action, int /*mods*/)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        ApplyKeyEvent(self->m_Impl->Raw, key, action == GLFW_PRESS || action == GLFW_REPEAT);
        if (key == GLFW_KEY_F11 && action == GLFW_PRESS)
            self->ToggleFullscreen();
    }

    void Window::OnMouseButton(GLFWwindow* handle, int button, int action, int /*mods*/)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        ApplyMouseButtonEvent(self->m_Impl->Raw, button, action == GLFW_PRESS);
    }

    void Window::OnMouseMove(GLFWwindow* handle, double x, double y)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        ApplyCursorEvent(self->m_Impl->Raw, x, y);
    }

    void Window::OnMouseScroll(GLFWwindow* handle, double x, double y)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        ApplyScrollEvent(self->m_Impl->Raw, x, y);
    }

    void Window::OnFocus(GLFWwindow* handle, int focused)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        ApplyFocus(self->m_Impl->Raw, focused != 0);
    }

    void Window::OnCursorEnter(GLFWwindow* handle, int entered)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        ApplyCursorEnter(self->m_Impl->Raw, entered != 0);
    }
}
