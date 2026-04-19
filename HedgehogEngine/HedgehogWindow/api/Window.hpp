#pragma once

#include "HedgehogEngine/HedgehogWindow/api/InputState.hpp"

#include <functional>

struct GLFWwindow;

namespace HW
{
    struct WindowDesc;

    class Window
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

    private:
        static void OnFramebufferResize(GLFWwindow* handle, int width, int height);
        static void OnKey(GLFWwindow* handle, int key, int scancode, int action, int mods);
        static void OnMouseButton(GLFWwindow* handle, int button, int action, int mods);
        static void OnMouseMove(GLFWwindow* handle, double x, double y);
        static void OnMouseScroll(GLFWwindow* handle, double x, double y);

    private:
        GLFWwindow*           m_Handle    = nullptr;
        InputState            m_InputState;
        bool                  m_Resized   = false;
        std::function<bool()> m_GuiCallback;
    };
}
