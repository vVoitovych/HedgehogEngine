#pragma once

#include "Panels/ContentIcons.hpp"
#include "Panels/EditorIcons.hpp"

#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogExtract/api/MeshBounds.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogInput/api/GameInputRegion.hpp"
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

namespace HedgehogScripting
{
    class ScriptDebugger;
    class ScriptSystem;
}

namespace Editor
{
    class EditorGui;
    class ImGuiLayer;

    // Chooses the project the editor opens (Project/StartupProject: --project's value, else, when
    // useRecentProjects, the most recent project that still exists, else the dev tree's default)
    // and makes it the project root, before an EditorApplication or game mode builds the engine.
    // Returns false, with one error, for a --project that holds no Project.yaml.
    [[nodiscard]] bool SelectStartupProject(const std::string& projectArgument, bool useRecentProjects);

    class EditorApplication
    {
    public:
        // recordRecentProject: the interactive editor puts its project at the front of the recent
        // list; automated runs (--smoke-test, --benchmark) leave the list as it is.
        explicit EditorApplication(HedgehogEngine::WindowMode windowMode = HedgehogEngine::WindowMode::Windowed,
                                   bool recordRecentProject = false);
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
        float GetSceneAspectRatio() const;

        float StepFrame();
        void  Render();
        void  PickAndHighlight(const HX::RenderCamera& sceneCamera);
        void  LoadBenchmarkScene(const std::string& sceneFile);

    private:
        HedgehogEngine::WindowMode m_WindowMode;
        bool                       m_RecordRecentProject = false;

        std::unique_ptr<HedgehogEngine::Engine>   m_Context;
        std::unique_ptr<Renderer::Renderer> m_Renderer;
        std::unique_ptr<ImGuiLayer>         m_ImGui;
        std::unique_ptr<EditorGui>          m_EditorGui;
        HedgehogScripting::ScriptSystem*    m_ScriptSystem = nullptr; // owned by the engine's ECS
        // Only when engine_settings.yaml enables the Lua debugger; goes before the script system.
        std::unique_ptr<HedgehogScripting::ScriptDebugger> m_ScriptDebugger;

        // The Content panel's pictures, uploaded once the device exists, and their ImGui ids.
        ContentIcons   m_ContentIcons;
        ContentIconIds m_ContentIconIds = {};
        // The editor's line icons, likewise.
        EditorIcons   m_EditorIcons;
        EditorIconIds m_EditorIconIds = {};
        // Which mouse buttons the game holds from presses that began on the Game tab.
        HInput::GameInputGate m_GameInputGate;
        // And those the editor camera holds from presses that began on the Scene tab.
        HInput::GameInputGate m_SceneInputGate;

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
