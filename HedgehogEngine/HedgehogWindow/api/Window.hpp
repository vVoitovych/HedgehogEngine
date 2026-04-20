#pragma once

#include "HedgehogEngine/HedgehogWindow/api/HedgehogWindowApi.hpp"
#include "HedgehogEngine/HedgehogWindow/api/InputState.hpp"

#include <functional>

struct GLFWwindow;

namespace HW
{
    struct WindowDesc;

    class HEDGEHOG_WINDOW_API Window
    {
    public:
        explicit Window(const WindowDesc& desc);
        ~Window();

        Window(const Window&)            = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&)                 = delete;
        Window& operator=(Window&&)      = delete;

        bool ShouldClose() const;

        bool IsResized() const;
        void ResetResizedFlag();

        void GetFramebufferSize(int& outWidth, int& outHeight) const;

        const InputState& GetInputState() const;
        InputState&       GetInputState();

        void SetIcon(int width, int height, unsigned char* data);
        void SetGuiCallback(std::function<bool()> callback);

        GLFWwindow*       GetNativeHandle();
        const GLFWwindow* GetNativeHandle() const;

        // Returns the native OS window handle (HWND on Win32).
        void* GetNativeOsHandle() const;

        // Returns the Vulkan instance extensions required by the windowing system.
        // Pointer is valid for the lifetime of the process.
        const char** GetVulkanExtensions(uint32_t& outCount) const;

    private:
        static void OnFramebufferResize(GLFWwindow* handle, int width, int height);
        static void OnKey(GLFWwindow* handle, int key, int scancode, int action, int mods);
        static void OnMouseButton(GLFWwindow* handle, int button, int action, int mods);
        static void OnMouseMove(GLFWwindow* handle, double x, double y);
        static void OnMouseScroll(GLFWwindow* handle, double x, double y);

    private:
        struct Impl;
        Impl* m_Impl = nullptr;
    };
}
