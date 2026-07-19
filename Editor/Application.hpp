#pragma once

#include <cstdint>
#include <memory>

namespace HedgehogEngine
{
    class Engine;
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

        // Loads the benchmark scene, renders warmupFrames untimed, then
        // measures measureFrames and logs per-pass and frame-time statistics.
        void RunBenchmark(uint32_t warmupFrames, uint32_t measureFrames);

    private:
        void  Init();
        void  MainLoop(uint32_t maxFrames);
        void  Cleanup();
        float GetFrameTime();

        float stepFrame();
        void  loadBenchmarkScene();

    private:
        std::unique_ptr<HedgehogEngine::Engine>   m_Context;
        std::unique_ptr<Renderer::Renderer> m_Renderer;
        std::unique_ptr<EditorGui>          m_EditorGui;
    };
}
