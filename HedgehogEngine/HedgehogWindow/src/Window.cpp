#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "HedgehogEngine/HedgehogWindow/api/DisplayMode.hpp"
#include "HedgehogEngine/HedgehogWindow/api/RawInputEvents.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowDesc.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowPolling.hpp"

#include "Logger/api/Logger.hpp"

#include "GLFW/glfw3.h"

#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include "GLFW/glfw3native.h"
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
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
        std::vector<ClientRect> CaptionAreas;      // what drags the window
        bool                    OnMonitor = false; // borderless or fullscreen: the window's own frame rules
#ifdef _WIN32
        LONG_PTR                GlfwProc = 0; // GLFW's window procedure, which ours forwards to
#endif
    };

    struct Window::Impl
    {
        GLFWwindow*           Handle         = nullptr;
        std::string           Title;
        RawInput              Raw;
        bool                  Resized        = false;
        WindowMode            Mode           = WindowMode::Windowed;
        bool                  SavedMaximized = false;
        int                   SavedX         = 0;
        int                   SavedY         = 0;
        int                   SavedWidth     = 1366;
        int                   SavedHeight    = 768;
        CustomTitleBar        TitleBar;
        CursorMode            RequestedCursor = CursorMode::Normal;
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
            if (bar->OnMonitor)
            {
                return CallWindowProcW(glfwProc, hwnd, message, wParam, lParam);
            }
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

        m_Impl->SavedX      = desc.X;
        m_Impl->SavedY      = desc.Y;
        m_Impl->SavedWidth  = desc.Width;
        m_Impl->SavedHeight = desc.Height;

        // A fullscreen window is created windowed and then made borderless, so it starts in the mode
        // GetWindowMode reports and the desktop's display mode is never changed.
        m_Impl->Handle = glfwCreateWindow(desc.Width, desc.Height, desc.Title.c_str(), nullptr, nullptr);
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
        if (desc.Fullscreen)
        {
            SetWindowMode(WindowMode::Borderless);
        }

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

    namespace
    {
        VideoMode ToVideoMode(const GLFWvidmode& mode)
        {
            return { mode.width, mode.height, mode.refreshRate };
        }

        // On a monitor: that one. Windowed: the monitor the window overlaps most, else the primary.
        GLFWmonitor* FindWindowMonitor(GLFWwindow* window)
        {
            if (GLFWmonitor* current = glfwGetWindowMonitor(window))
            {
                return current;
            }

            int x      = 0;
            int y      = 0;
            int width  = 0;
            int height = 0;
            glfwGetWindowPos(window, &x, &y);
            glfwGetWindowSize(window, &width, &height);

            int                 count    = 0;
            GLFWmonitor** const monitors = glfwGetMonitors(&count);
            GLFWmonitor*        best     = glfwGetPrimaryMonitor();
            int64_t             bestArea = 0;
            for (int i = 0; i < count; ++i)
            {
                const GLFWvidmode* const mode = glfwGetVideoMode(monitors[i]);
                if (mode == nullptr)
                {
                    continue;
                }
                int monitorX = 0;
                int monitorY = 0;
                glfwGetMonitorPos(monitors[i], &monitorX, &monitorY);
                const int64_t overlapWidth  = std::min(x + width, monitorX + mode->width) - std::max(x, monitorX);
                const int64_t overlapHeight = std::min(y + height, monitorY + mode->height) - std::max(y, monitorY);
                if (overlapWidth > 0 && overlapHeight > 0 && overlapWidth * overlapHeight > bestArea)
                {
                    bestArea = overlapWidth * overlapHeight;
                    best     = monitors[i];
                }
            }
            return best;
        }
    }

    namespace
    {
        // GLFW sizes the window's frame for a normal caption, which a custom title bar takes back
        // into the client, so the client is corrected to the size asked for.
        void PlaceWindowed(GLFWwindow* window, int x, int y, int width, int height)
        {
            glfwSetWindowMonitor(window, nullptr, x, y, width, height, 0);
            int clientWidth  = 0;
            int clientHeight = 0;
            glfwGetWindowSize(window, &clientWidth, &clientHeight);
            if (clientWidth != width || clientHeight != height)
            {
                glfwSetWindowSize(window, 2 * width - clientWidth, 2 * height - clientHeight);
            }
        }
    }

    std::vector<VideoMode> Window::GetVideoModes() const
    {
        int                      count = 0;
        const GLFWvidmode* const modes = glfwGetVideoModes(FindWindowMonitor(m_Impl->Handle), &count);
        std::vector<VideoMode>   result;
        for (int i = 0; modes != nullptr && i < count; ++i)
        {
            result.push_back(ToVideoMode(modes[i]));
        }
        return NormalizeVideoModes(std::move(result));
    }

    void Window::SetWindowMode(WindowMode mode, int width, int height)
    {
        Impl&      impl    = *m_Impl;
        const bool hasSize = width > 0 && height > 0;
        if (mode == impl.Mode && (mode == WindowMode::Borderless || !hasSize))
        {
            return;
        }

        if (impl.Mode == WindowMode::Windowed)
        {
            // A minimized or maximized window is restored first, so the saved geometry is its
            // restored one (a minimized window sits far off-screen at no size).
            if (glfwGetWindowAttrib(impl.Handle, GLFW_ICONIFIED))
            {
                glfwRestoreWindow(impl.Handle);
            }
            impl.SavedMaximized = IsMaximized();
            if (impl.SavedMaximized)
            {
                glfwRestoreWindow(impl.Handle);
            }
            int x      = 0;
            int y      = 0;
            int width  = 0;
            int height = 0;
            glfwGetWindowPos(impl.Handle, &x, &y);
            glfwGetWindowSize(impl.Handle, &width, &height);
            if (width > 0 && height > 0)
            {
                impl.SavedX      = x;
                impl.SavedY      = y;
                impl.SavedWidth  = width;
                impl.SavedHeight = height;
            }
        }
        else if (mode == WindowMode::Borderless && impl.Mode == WindowMode::Fullscreen)
        {
            // Leave the display mode we set, so the monitor's current mode below is the desktop's.
            impl.TitleBar.OnMonitor = false;
            PlaceWindowed(impl.Handle, impl.SavedX, impl.SavedY, impl.SavedWidth, impl.SavedHeight);
        }

        impl.TitleBar.OnMonitor = mode != WindowMode::Windowed;
        const int autoIconify = mode == WindowMode::Fullscreen ? GLFW_TRUE : GLFW_FALSE;
        glfwSetWindowAttrib(impl.Handle, GLFW_AUTO_ICONIFY, autoIconify);

        switch (mode)
        {
        case WindowMode::Windowed:
            if (hasSize)
            {
                int workX      = 0;
                int workY      = 0;
                int workWidth  = 0;
                int workHeight = 0;
                glfwGetMonitorWorkarea(FindWindowMonitor(impl.Handle), &workX, &workY, &workWidth, &workHeight);
                // A window larger than the work area keeps its corner on it, so its title bar stays reachable.
                PlaceWindowed(impl.Handle, workX + std::max(0, (workWidth - width) / 2),
                              workY + std::max(0, (workHeight - height) / 2), width, height);
                impl.SavedMaximized = false;
            }
            else
            {
                PlaceWindowed(impl.Handle, impl.SavedX, impl.SavedY, impl.SavedWidth, impl.SavedHeight);
                if (impl.SavedMaximized)
                {
                    glfwMaximizeWindow(impl.Handle);
                }
            }
            break;
        case WindowMode::Borderless:
        {
            GLFWmonitor* const       monitor = FindWindowMonitor(impl.Handle);
            const GLFWvidmode* const current = glfwGetVideoMode(monitor);
            if (current != nullptr)
            {
                glfwSetWindowMonitor(impl.Handle, monitor, 0, 0, current->width, current->height,
                                     current->refreshRate);
            }
            break;
        }
        case WindowMode::Fullscreen:
        {
            GLFWmonitor* const       monitor = FindWindowMonitor(impl.Handle);
            const GLFWvidmode* const current = glfwGetVideoMode(monitor);
            VideoMode                chosen  = current != nullptr ? ToVideoMode(*current) : VideoMode{};
            // Without a size the monitor keeps its current resolution.
            const int wantWidth  = hasSize ? width : chosen.Width;
            const int wantHeight = hasSize ? height : chosen.Height;
            if (const auto found = ChooseVideoMode(GetVideoModes(), wantWidth, wantHeight))
            {
                chosen = *found;
            }
            glfwSetWindowMonitor(impl.Handle, monitor, 0, 0, chosen.Width, chosen.Height, chosen.RefreshRate);
            break;
        }
        }

        impl.Mode            = mode;
        impl.Raw.CursorKnown = false; // the cursor jumps
        impl.Resized         = true;  // a switch keeping the pixel size fires no size callback
    }

    WindowMode Window::GetWindowMode() const
    {
        return m_Impl->Mode;
    }

    void Window::ToggleFullscreen()
    {
        SetWindowMode(m_Impl->Mode == WindowMode::Windowed ? WindowMode::Borderless : WindowMode::Windowed);
    }

    bool Window::IsFullscreen() const
    {
        return m_Impl->Mode != WindowMode::Windowed;
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
        // A borderless or fullscreen window already covers its monitor.
        if (m_Impl->Mode != WindowMode::Windowed)
        {
            return;
        }
        if (IsMaximized())
            glfwRestoreWindow(m_Impl->Handle);
        else
            glfwMaximizeWindow(m_Impl->Handle);
    }

    bool Window::IsMaximized() const
    {
        return glfwGetWindowAttrib(m_Impl->Handle, GLFW_MAXIMIZED) != 0;
    }

    void Window::SetCursorMode(CursorMode mode)
    {
        m_Impl->RequestedCursor = mode;
        // Locking and unlocking move the cursor, so its next event sets the position without a delta.
        m_Impl->Raw.CursorKnown = false;
        HW::SetCursorMode(m_Impl->Handle, ResolveCursorMode(mode, m_Impl->Raw.Focused));
    }

    CursorMode Window::GetCursorMode() const
    {
        return m_Impl->RequestedCursor;
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
        // The cursor jumps as GLFW takes or releases a lock, so neither this frame's motion so far nor
        // the next event's counts.
        self->m_Impl->Raw.CursorKnown = false;
        self->m_Impl->Raw.CursorDelta = HM::Vector2(0.0f, 0.0f);
        // GLFW itself releases a locked or confined cursor on focus loss and takes it again on focus
        // (setting it here too would make it record the centred cursor as where to put it back).
        if (self->m_Impl->RequestedCursor == CursorMode::Hidden)
        {
            HW::SetCursorMode(handle, ResolveCursorMode(CursorMode::Hidden, focused != 0));
        }
    }

    void Window::OnCursorEnter(GLFWwindow* handle, int entered)
    {
        auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(handle));
        ApplyCursorEnter(self->m_Impl->Raw, entered != 0);
    }
}
