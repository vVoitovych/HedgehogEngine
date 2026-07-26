#pragma once

#include "HedgehogRenderer/Renderer.hpp"

#include <cstdint>
#include <memory>

namespace HedgehogEngine
{
    class Engine;
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

        // Headless/game-mode proof (Phase 6): exactly one view (game_view.rgq), composed straight
        // to the swapchain (present_direct.rgq — no GuiPass); no EditorGui is ever constructed and
        // BeginGui() is never called, so no ImGui context exists for the process's lifetime. Renders
        // `frames` frames and returns — same "batch mode, then exit" shape as Run(maxFrames).
        void RunGameMode(uint32_t frames);

    private:
        void  Init();
        void  InitGameMode();
        void  MainLoop(uint32_t maxFrames);
        void  Cleanup();
        float GetFrameTime();

        float StepFrame();
        float StepGameFrame();
        void  LoadBenchmarkScene();

    private:
        std::unique_ptr<HedgehogEngine::Engine>   m_Context;
        std::unique_ptr<Renderer::Renderer> m_Renderer;
        std::unique_ptr<EditorGui>          m_EditorGui;

        Renderer::ViewHandle m_SceneView = Renderer::INVALID_VIEW;
        Renderer::ViewHandle m_GameView  = Renderer::INVALID_VIEW;
        Renderer::ViewHandle m_MainView  = Renderer::INVALID_VIEW; // game mode's single view
    };
}
