#pragma once

#include <cstdint>
#include <memory>

namespace HedgehogEngine
{
    class HedgehogEngine;
}

namespace Renderer
{
    class Renderer;
}

namespace Editor
{
    class EditorGui;

    class EditorApplication
    {
    public:
        EditorApplication();
        ~EditorApplication();

        EditorApplication(const EditorApplication&)            = delete;
        EditorApplication& operator=(const EditorApplication&) = delete;
        EditorApplication(EditorApplication&&)                 = delete;
        EditorApplication& operator=(EditorApplication&&)      = delete;

        // maxFrames == 0 runs until the window is closed; a positive value
        // renders that many frames and exits (used by the --smoke-test mode).
        void Run(uint32_t maxFrames = 0);

    private:
        void  Init();
        void  MainLoop(uint32_t maxFrames);
        void  Cleanup();
        float GetFrameTime();

    private:
        std::unique_ptr<HedgehogEngine::HedgehogEngine>   m_Context;
        std::unique_ptr<Renderer::Renderer> m_Renderer;
        std::unique_ptr<EditorGui>          m_EditorGui;
    };
}
