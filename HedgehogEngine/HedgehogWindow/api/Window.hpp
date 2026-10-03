#pragma once

#include "HedgehogEngine/HedgehogWindow/api/HedgehogWindowApi.hpp"
#include "HedgehogEngine/HedgehogWindow/api/InputState.hpp"
#include "HedgehogEngine/HedgehogWindow/api/RawInput.hpp"

#include <functional>
#include <memory>

struct GLFWwindow;

namespace HW
{
    struct WindowDesc;

    class Window
    {
    public:
        HEDGEHOG_WINDOW_API explicit Window(const WindowDesc& desc);
        HEDGEHOG_WINDOW_API ~Window();

        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&)                 = delete;
        Window& operator=(Window&&)      = delete;

        HEDGEHOG_WINDOW_API bool ShouldClose() const;

        HEDGEHOG_WINDOW_API bool IsResized() const;
        HEDGEHOG_WINDOW_API void ResetResizedFlag();

        // Blocks until at least one event is available (used while minimized to avoid busy-waiting).
        HEDGEHOG_WINDOW_API void WaitEvents() const;

        HEDGEHOG_WINDOW_API void ToggleFullscreen();
        HEDGEHOG_WINDOW_API bool IsFullscreen() const;

        HEDGEHOG_WINDOW_API void GetFramebufferSize(int& outWidth, int& outHeight) const;

        HEDGEHOG_WINDOW_API const InputState& GetInputState() const;
        HEDGEHOG_WINDOW_API InputState&       GetInputState();

        // Every key, mouse button, cursor move and scroll the window has received, unfiltered: the
        // GUI callback gates only the legacy InputState. BeginInputFrame starts the frame's deltas.
        HEDGEHOG_WINDOW_API const RawInput& GetRawInput() const;
        HEDGEHOG_WINDOW_API RawInput&       GetRawInput();
        HEDGEHOG_WINDOW_API void            BeginInputFrame();

        HEDGEHOG_WINDOW_API void SetIcon(int width, int height, unsigned char* data);
        HEDGEHOG_WINDOW_API void SetGuiCallback(std::function<bool()> callback);

        HEDGEHOG_WINDOW_API GLFWwindow*       GetNativeHandle();
        HEDGEHOG_WINDOW_API const GLFWwindow* GetNativeHandle() const;

        // Returns the native OS window handle (HWND on Win32).
        HEDGEHOG_WINDOW_API void* GetNativeOsHandle() const;

    private:
        static void OnFramebufferResize(GLFWwindow* handle, int width, int height);
        static void OnKey(GLFWwindow* handle, int key, int scancode, int action, int mods);
        static void OnMouseButton(GLFWwindow* handle, int button, int action, int mods);
        static void OnMouseMove(GLFWwindow* handle, double x, double y);
        static void OnMouseScroll(GLFWwindow* handle, double x, double y);
        static void OnFocus(GLFWwindow* handle, int focused);
        static void OnCursorEnter(GLFWwindow* handle, int entered);

    private:
        struct Impl;
        std::unique_ptr<Impl> m_Impl;
    };
}
