#include "Application.hpp"
#include "EditorGui.hpp"

#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogRenderer/Renderer.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <vector>

namespace
{
    constexpr const char* GRAPHS_DIR = "engine://HedgehogEngine/HedgehogRenderer/assets/Graphs/";

    // Every view/asset name the editor and headless game mode reference, in one place — see
    // workflow/current-plan.md, Phase 6 step 32.
    struct GraphAssets
    {
        static std::string FrameDefault()      { return std::string(GRAPHS_DIR) + "frame_default.rgq"; }
        static std::string SceneView()         { return std::string(GRAPHS_DIR) + "scene_view.rgq"; }
        static std::string GameView()          { return std::string(GRAPHS_DIR) + "game_view.rgq"; }
        static std::string CompositionEditor() { return std::string(GRAPHS_DIR) + "composition_editor.rgq"; }
        static std::string PresentDirect()     { return std::string(GRAPHS_DIR) + "present_direct.rgq"; }
    };

    constexpr const char* SCENE_VIEW_NAME = "scene";
    constexpr const char* GAME_VIEW_NAME  = "game";
    // present_direct.rgq's PresentPass hardcodes "views:main" as its blit source — this name and
    // that asset's reference must stay in sync.
    constexpr const char* MAIN_VIEW_NAME  = "main";
}

namespace Editor
{
    EditorApplication::EditorApplication()  = default;
    EditorApplication::~EditorApplication() = default;

    void EditorApplication::Run(uint32_t maxFrames)
    {
        Init();
        MainLoop(maxFrames);
    }

    void EditorApplication::Init()
    {
        m_Context   = std::make_unique<HedgehogEngine::Engine>();

        auto& engineContext = m_Context->GetEngineContext();
        m_Renderer  = std::make_unique<Renderer::Renderer>(
            m_Context->GetWindowContext().GetWindow(),
            engineContext.GetSettings(),
            engineContext.GetFileSystem());
        m_EditorGui = std::make_unique<EditorGui>(*m_Context);

        if (!m_Renderer->SetFramePipeline(GraphAssets::FrameDefault()))
            LOGERROR("Editor: failed to load the frame pipeline; rendering will not start.");

        Renderer::ViewDesc sceneDesc;
        sceneDesc.Name          = SCENE_VIEW_NAME;
        sceneDesc.PipelineAsset = GraphAssets::SceneView();
        if (const auto view = m_Renderer->CreateView(sceneDesc))
            m_SceneView = *view;
        else
            LOGERROR("Editor: failed to create the scene view.");

        Renderer::ViewDesc gameDesc;
        gameDesc.Name          = GAME_VIEW_NAME;
        gameDesc.PipelineAsset = GraphAssets::GameView();
        if (const auto view = m_Renderer->CreateView(gameDesc))
            m_GameView = *view;
        else
            LOGERROR("Editor: failed to create the game view.");

        m_Renderer->SetMainView(m_SceneView);

        // Called last so GuiPass sees the final view set — see Renderer::SetCompositionPipeline.
        if (!m_Renderer->SetCompositionPipeline(GraphAssets::CompositionEditor()))
            LOGERROR("Editor: failed to load the composition pipeline; nothing will reach the screen.");

        // WantCaptureMouse is true even over the scene image (it's an ImGui window); exempt it.
        m_Context->GetWindowContext().GetWindow().SetGuiCallback([this]()
        {
            return ImGui::GetIO().WantCaptureMouse && !m_EditorGui->IsSceneViewHovered();
        });

        LOGINFO("Editor initialized");
    }

    void EditorApplication::InitGameMode()
    {
        m_Context  = std::make_unique<HedgehogEngine::Engine>();

        auto& engineContext = m_Context->GetEngineContext();
        m_Renderer = std::make_unique<Renderer::Renderer>(
            m_Context->GetWindowContext().GetWindow(),
            engineContext.GetSettings(),
            engineContext.GetFileSystem());
        // No EditorGui, no BeginGui() call anywhere in this mode's frame loop — GuiPass (the only
        // ImGui-touching type in the renderer) is never even a node in present_direct.rgq, so no
        // ImGui context exists for this process's lifetime. See workflow/current-plan.md, Phase 6.

        if (!m_Renderer->SetFramePipeline(GraphAssets::FrameDefault()))
            LOGERROR("Game mode: failed to load the frame pipeline; rendering will not start.");

        int windowWidth = 0, windowHeight = 0;
        m_Context->GetWindowContext().GetWindow().GetFramebufferSize(windowWidth, windowHeight);

        Renderer::ViewDesc mainDesc;
        mainDesc.Name          = MAIN_VIEW_NAME;
        mainDesc.PipelineAsset = GraphAssets::GameView();
        mainDesc.Width         = static_cast<uint32_t>(windowWidth);
        mainDesc.Height        = static_cast<uint32_t>(windowHeight);
        if (const auto view = m_Renderer->CreateView(mainDesc))
            m_MainView = *view;
        else
            LOGERROR("Game mode: failed to create the main view.");

        m_Renderer->SetMainView(m_MainView);

        if (!m_Renderer->SetCompositionPipeline(GraphAssets::PresentDirect()))
            LOGERROR("Game mode: failed to load the composition pipeline; nothing will reach the screen.");

        LOGINFO("Game mode initialized");
    }

    void EditorApplication::RunBenchmark(uint32_t warmupFrames, uint32_t measureFrames)
    {
        Init();
        LoadBenchmarkScene();

        LOGINFO("Benchmark: warming up for ", warmupFrames, " frame(s)...");
        auto& windowContext = m_Context->GetWindowContext();
        for (uint32_t i = 0; i < warmupFrames && !windowContext.ShouldClose(); ++i)
            StepFrame();

        LOGINFO("Benchmark: measuring ", measureFrames, " frame(s)...");
        m_Renderer->BeginFrameStatsCapture();

        std::vector<double> frameTimesMs;
        frameTimesMs.reserve(measureFrames);
        for (uint32_t i = 0; i < measureFrames && !windowContext.ShouldClose(); ++i)
            frameTimesMs.push_back(static_cast<double>(StepFrame()) * 1000.0);

        m_Renderer->EndFrameStatsCaptureAndLogReport();

        if (!frameTimesMs.empty())
        {
            std::vector<double> sorted = frameTimesMs;
            std::sort(sorted.begin(), sorted.end());

            const double avg = std::accumulate(sorted.begin(), sorted.end(), 0.0)
                             / static_cast<double>(sorted.size());
            const size_t p95Index = std::min(sorted.size() - 1,
                static_cast<size_t>(static_cast<double>(sorted.size()) * 0.95));

            char line[192];
            std::snprintf(line, sizeof(line),
                "Benchmark  | %-20s | %8.3f | %8.3f | %8.3f | %8.3f | %7zu (avg %.1f FPS)",
                "Frame(wall)", avg, sorted.front(), sorted.back(), sorted[p95Index],
                sorted.size(), avg > 0.0 ? 1000.0 / avg : 0.0);
            LOGINFO(line);
        }

        Cleanup();
    }

    void EditorApplication::MainLoop(uint32_t maxFrames)
    {
        uint32_t frameIndex = 0;
        while (!m_Context->GetWindowContext().ShouldClose()
            && (maxFrames == 0 || frameIndex < maxFrames))
        {
            ++frameIndex;
            StepFrame();
        }

        Cleanup();
    }

    void EditorApplication::RunGameMode(uint32_t frames)
    {
        InitGameMode();

        uint32_t frameIndex = 0;
        auto& windowContext = m_Context->GetWindowContext();
        while (!windowContext.ShouldClose() && (frames == 0 || frameIndex < frames))
        {
            ++frameIndex;
            StepGameFrame();
        }

        Cleanup();
    }

    float EditorApplication::StepFrame()
    {
        const float dt = GetFrameTime();
        m_Context->GetWindowContext().HandleInput();
        const bool tickGameLogic = (m_EditorGui->GetEditorMode() == EditorMode::Play);
        m_Context->UpdateContext(dt, m_Renderer->GetViewAspectRatio(m_SceneView),
                                 m_Renderer->GetViewAspectRatio(m_GameView), tickGameLogic);

        m_Renderer->BeginGui();
        m_EditorGui->Draw(*m_Context, m_Renderer->GetViewTextureId(m_SceneView),
                          m_Renderer->GetViewTextureId(m_GameView));
        // Only push a size for a view that was actually drawn (and therefore measured) this frame
        // — in tabbed mode the inactive tab's width/height are stale (or, before it's ever been
        // opened, still zero-initialized), and resizing a view to 0x0 asserts in the graph's
        // resource pool.
        if (m_EditorGui->IsSceneViewVisible())
            m_Renderer->SetViewSize(m_SceneView, m_EditorGui->GetSceneViewWidth(),
                                    m_EditorGui->GetSceneViewHeight());
        if (m_EditorGui->IsGameViewVisible())
            m_Renderer->SetViewSize(m_GameView, m_EditorGui->GetGameViewWidth(),
                                    m_EditorGui->GetGameViewHeight());
        m_Renderer->SetViewEnabled(m_SceneView, m_EditorGui->IsSceneViewVisible());
        m_Renderer->SetViewEnabled(m_GameView, m_EditorGui->IsGameViewVisible());
        m_Renderer->SetViewGizmo(m_SceneView, m_EditorGui->GetSelectedGizmoMatrix(*m_Context));

        auto& engineContext = m_Context->GetEngineContext();
        const auto& frameData = engineContext.GetFrameData();
        m_Renderer->SetViewCamera(m_SceneView, frameData.Camera);
        m_Renderer->SetViewCamera(m_GameView, frameData.GameCamera);

        m_Renderer->DrawFrame(frameData, engineContext.GetResourceCatalog(),
                              engineContext.GetSettings());
        return dt;
    }

    float EditorApplication::StepGameFrame()
    {
        const float dt = GetFrameTime();
        m_Context->GetWindowContext().HandleInput();

        // No separate scene/game aspect ratio split — there's only one view, and no editor flycam
        // to drive with a distinct aspect.
        const float aspect = m_Renderer->GetViewAspectRatio(m_MainView);
        m_Context->UpdateContext(dt, aspect, aspect, /*tickGameLogic*/ true);

        auto& engineContext = m_Context->GetEngineContext();
        const auto& frameData = engineContext.GetFrameData();
        m_Renderer->SetViewCamera(m_MainView, frameData.GameCamera);

        m_Renderer->DrawFrame(frameData, engineContext.GetResourceCatalog(),
                              engineContext.GetSettings());
        return dt;
    }

    void EditorApplication::LoadBenchmarkScene()
    {
        constexpr const char* BENCHMARK_SCENE = "assets://Scenes/benchmark.yaml";

        auto&       engineContext = m_Context->GetEngineContext();
        const auto& fileSystem    = engineContext.GetFileSystem();

        const auto physicalPath = fileSystem.ResolvePhysical(BENCHMARK_SCENE);
        if (!physicalPath || !engineContext.GetSceneManager().LoadScene(physicalPath->string()))
        {
            LOGWARNING("Benchmark: failed to load '", BENCHMARK_SCENE,
                       "'; measuring whatever scene is currently open instead.");
        }
    }

    void EditorApplication::Cleanup()
    {
        m_Renderer->Cleanup();
        m_Context->Cleanup();
    }

    float EditorApplication::GetFrameTime()
    {
        static auto s_PrevTime = std::chrono::high_resolution_clock::now();

        const auto  currentTime = std::chrono::high_resolution_clock::now();
        const float deltaTime   = std::chrono::duration<float, std::chrono::seconds::period>(
            currentTime - s_PrevTime).count();
        s_PrevTime = currentTime;
        return deltaTime;
    }
}
