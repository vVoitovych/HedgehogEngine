#pragma once

#include "HedgehogExtract/api/MeshBounds.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogRenderer/Views/View.hpp"

#include <cstdint>
#include <memory>
#include <string>

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
    class ImGuiLayer;

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

        // Loads sceneFile (under Assets/Scenes), renders warmupFrames untimed, then measures
        // measureFrames and logs per-pass and frame-time statistics.
        void RunBenchmark(uint32_t warmupFrames, uint32_t measureFrames, const std::string& sceneFile);

    private:
        void  Init();
        void  MainLoop(uint32_t maxFrames);
        void  Cleanup();
        float GetFrameTime();

        float StepFrame();
        void  Render();
        void  PickAndHighlight(const HX::RenderCamera& sceneCamera);
        void  LoadBenchmarkScene(const std::string& sceneFile);

    private:
        std::unique_ptr<HedgehogEngine::Engine>   m_Context;
        std::unique_ptr<Renderer::Renderer> m_Renderer;
        std::unique_ptr<ImGuiLayer>         m_ImGui;
        std::unique_ptr<EditorGui>          m_EditorGui;

        // RENDERING.md section 7: the scene extracted each frame, the scene view (the editor camera
        // into the scene panel's target) and the result view (the editor's UI into the window,
        // showing both panels). The game view is the scene's camera, redirected to the game panel's
        // target.
        HX::RenderScene     m_RenderScene;
        HX::MeshBoundsCache m_MeshBounds;
        Renderer::ViewId m_SceneView  = Renderer::INVALID_VIEW_ID;
        Renderer::ViewId m_ResultView = Renderer::INVALID_VIEW_ID;
    };
}
