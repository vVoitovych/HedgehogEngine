#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include <memory>

namespace HW
{
    class Window;
    class WindowManager;
}

namespace HedgehogEngine
{
    // How the main window opens. The interactive editor asks for Maximized. The automated runs
    // (--smoke-test, --game-mode, --benchmark) keep the fixed-size Windowed default so their
    // results stay comparable across machines and runs.
    enum class WindowMode
    {
        Windowed,
        Maximized
    };

    class WindowContext
    {
    public:
        HEDGEHOG_ENGINE_API explicit WindowContext(WindowMode mode);
        HEDGEHOG_ENGINE_API ~WindowContext();

        WindowContext(const WindowContext&)            = delete;
        WindowContext(WindowContext&&)                 = delete;
        WindowContext& operator=(const WindowContext&) = delete;
        WindowContext& operator=(WindowContext&&)      = delete;

        HEDGEHOG_ENGINE_API void HandleInput();

        HEDGEHOG_ENGINE_API bool ShouldClose() const;

        HEDGEHOG_ENGINE_API HW::Window&       GetWindow();
        HEDGEHOG_ENGINE_API const HW::Window& GetWindow() const;

    private:
        std::unique_ptr<HW::WindowManager> m_WindowManager;
        HW::Window*                        m_Window = nullptr;
    };
}
