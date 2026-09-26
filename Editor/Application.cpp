#include "Application.hpp"
#include "EditorGui.hpp"
#include "ImGuiLayer.hpp"

#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/LayerSettings.hpp"
#include "HedgehogCommon/api/Camera.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"
#include "HedgehogExtract/api/ScenePicker.hpp"
#include "HedgehogRenderer/Renderer.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include "imgui.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <vector>

namespace Editor
{
    namespace
    {
        constexpr const char* ENGINE_SETTINGS_PATH = "engine://engine_settings.yaml";

        constexpr float RADIANS_TO_DEGREES = 57.2957795f;

        static_assert(HX::EDITOR_LAYER == HedgehogSettings::LayerSettings::EDITOR_LAYER,
                      "The extracted scene and the layer settings must agree on the editor layer.");

        // The render targets the scene and game panels show, sized to their panels.
        constexpr const char* SCENE_TARGET = "scene";
        constexpr const char* GAME_TARGET  = "game";

        // The editor camera as a view, drawn with the scene graph into the scene panel's target. It
        // outranks scene cameras, so the shared shadows fit it.
        Renderer::ViewDesc MakeSceneView(const HedgehogEngine::Camera& camera)
        {
            HX::RenderCamera renderCamera;
            renderCamera.WorldMatrix = camera.GetViewMatrix().Inverse();
            renderCamera.Fov         = camera.GetFov() * RADIANS_TO_DEGREES;
            renderCamera.NearPlane   = camera.GetNearPlane();
            renderCamera.FarPlane    = camera.GetFarPlane();

            Renderer::ViewDesc desc;
            desc.Camera    = renderCamera;
            desc.Targets   = { SCENE_TARGET };
            desc.GraphName = "scene";
            desc.Priority  = 100;
            return desc;
        }

        // The editor's UI into the window. It has no camera, and it reads both panels' targets, so
        // it is ordered after the views that write them.
        Renderer::ViewDesc MakeResultView()
        {
            Renderer::ViewDesc desc;
            desc.Targets   = { std::string(Renderer::RenderTargetRegistry::MAIN_TARGET) };
            desc.Reads     = { SCENE_TARGET, GAME_TARGET };
            desc.GraphName = "result";
            desc.Priority  = 100;
            return desc;
        }

    }

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

        // Engine settings must load before the renderer is constructed: they decide the size of
        // GPU resources it creates there. Loading afterwards leaves the settings dirty for the
        // first frame and forces a resize of resources that have never been rendered to.
        auto&       settings   = engineContext.GetSettings();
        const auto& fileSystem = engineContext.GetFileSystem();
        if (!fileSystem.Exists(ENGINE_SETTINGS_PATH))
        {
            LOGINFO("No engine settings file yet, using defaults.");
        }
        else if (!settings.Load(ENGINE_SETTINGS_PATH, fileSystem))
        {
            LOGWARNING("Engine settings could not be read, using defaults.");
        }
        // Nothing to resize: the renderer below reads these values when it creates its resources.
        settings.CleanDirtyState();

        m_Renderer  = std::make_unique<Renderer::Renderer>(
            m_Context->GetWindowContext().GetWindow(),
            engineContext.GetSettings(),
            engineContext.GetFileSystem(),
            Renderer::RendererPaths::RenderGraphOnly);
        m_ImGui     = std::make_unique<ImGuiLayer>(m_Context->GetWindowContext().GetWindow());
        m_EditorGui = std::make_unique<EditorGui>(*m_Context);

        // The panels' targets: zero-sized until their tabs are first drawn.
        for (const char* target : { SCENE_TARGET, GAME_TARGET })
        {
            const Renderer::RenderTargetResult declared = m_Renderer->DeclareTarget(
                { target, RHI::Format::R16G16B16A16Unorm, Renderer::RGSizePolicy::MakeAbsolute(0, 0) });
            if (!declared.Success)
                LOGERROR("Editor: ", declared.Message);
        }
        m_SceneView  = m_Renderer->CreateView(MakeSceneView(engineContext.GetCamera()));
        m_ResultView = m_Renderer->CreateView(MakeResultView());

        // WantCaptureMouse is true even over the scene image (it's an ImGui window); exempt it.
        m_Context->GetWindowContext().GetWindow().SetGuiCallback([this]()
        {
            return ImGui::GetIO().WantCaptureMouse && !m_EditorGui->IsSceneViewHovered();
        });

        LOGINFO("Editor initialized");
    }

    void EditorApplication::RunBenchmark(uint32_t warmupFrames, uint32_t measureFrames, const std::string& sceneFile)
    {
        Init();
        LoadBenchmarkScene(sceneFile);

        LOGINFO("Benchmark: warming up for ", warmupFrames, " frame(s)...");
        auto& windowContext = m_Context->GetWindowContext();
        for (uint32_t i = 0; i < warmupFrames && !windowContext.ShouldClose(); ++i)
            StepFrame();

        LOGINFO("Benchmark: render instances = ", m_RenderScene.Instances.size());

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

    float EditorApplication::StepFrame()
    {
        const float dt = GetFrameTime();
        m_Context->GetWindowContext().HandleInput();
        m_Context->UpdateContext(dt, m_Renderer->GetAspectRatio());

        m_ImGui->BeginFrame(*m_Renderer);
        ViewportImages images;
        images.Scene          = m_ImGui->GetTextureId(SCENE_TARGET, m_Renderer->GetTargetTexture(SCENE_TARGET));
        images.Game           = m_ImGui->GetTextureId(GAME_TARGET, m_Renderer->GetTargetTexture(GAME_TARGET));
        images.GraphPassCount = m_Renderer->GetLastFramePassCount();
        m_EditorGui->Draw(*m_Context, images);
        m_ImGui->EndFrame();

        Render();
        return dt;
    }

    // Extract, then render every view through the render graph (RENDERING.md section 7).
    void EditorApplication::Render()
    {
        auto& engineContext = m_Context->GetEngineContext();

        m_MeshBounds.Update(engineContext.GetResourceCatalog());
        m_RenderScene.Clear();
        HX::SceneExtractor{}.Extract(engineContext.GetECS(), *engineContext.GetRenderSystem(),
                                     *engineContext.GetLightSystem(), *engineContext.GetCameraSystem(),
                                     m_RenderScene, m_MeshBounds.GetBounds());

        const Renderer::ViewDesc sceneView = MakeSceneView(engineContext.GetCamera());
        PickAndHighlight(*sceneView.Camera);

        [[maybe_unused]] const bool updated = m_Renderer->UpdateView(m_SceneView, sceneView);
        assert(updated && "EditorApplication: the scene view was not created.");

        // A hidden panel is 0x0: its view is dropped and its passes never declared.
        (void)m_Renderer->ResizeTarget(SCENE_TARGET, m_EditorGui->GetSceneViewWidth(),
                                       m_EditorGui->GetSceneViewHeight());
        (void)m_Renderer->ResizeTarget(GAME_TARGET, m_EditorGui->GetGameViewWidth(),
                                       m_EditorGui->GetGameViewHeight());

        // The game view: a camera that would draw to the window draws into the game panel instead,
        // leaving the camera itself untouched.
        for (const HX::RenderCamera& camera : m_RenderScene.Cameras)
        {
            if (camera.TargetMode == HX::CameraTargetMode::Main)
                m_Renderer->SetCameraTargetOverride(camera.SourceId, { GAME_TARGET });
        }

        m_Renderer->SyncResources(engineContext.GetResourceCatalog());
        m_Renderer->RenderFrame(m_RenderScene, engineContext.GetSettings(), m_ImGui->GetUiCallback());
    }

    // Picking reads only the extracted scene (ScenePicker). The selection is drawn by adding a copy of
    // its instance on the editor layer, which only the scene view's Gizmo pass draws: the renderer
    // has no notion of a selection.
    void EditorApplication::PickAndHighlight(const HX::RenderCamera& sceneCamera)
    {
        if (const std::optional<ViewportPoint> click = m_EditorGui->GetScenePick())
        {
            const float aspect = static_cast<float>(m_EditorGui->GetSceneViewWidth())
                               / static_cast<float>(std::max(1u, m_EditorGui->GetSceneViewHeight()));
            const HX::Ray ray = HX::MakePickRay(sceneCamera, aspect, click->U, click->V);
            const std::optional<uint64_t> picked = HX::PickInstance(m_RenderScene, ray);
            m_EditorGui->SetSelectedEntity(picked ? std::optional<ECS::Entity>(static_cast<ECS::Entity>(*picked))
                                                  : std::nullopt);
        }

        const std::optional<ECS::Entity> selected = m_EditorGui->GetSelectedEntity();
        if (!selected)
            return;
        const auto it = std::find_if(m_RenderScene.Instances.begin(), m_RenderScene.Instances.end(),
            [&](const HX::RenderInstance& instance) { return instance.SourceId == static_cast<uint64_t>(*selected); });
        if (it == m_RenderScene.Instances.end())
            return; // not drawn: a light, a camera, or a hidden object
        HX::RenderInstance gizmo = *it;
        gizmo.Layer = HX::EDITOR_LAYER;
        m_RenderScene.Instances.push_back(gizmo);
    }

    void EditorApplication::LoadBenchmarkScene(const std::string& sceneFile)
    {
        const std::string scenePath = "assets://Scenes/" + sceneFile;

        auto&       engineContext = m_Context->GetEngineContext();
        const auto& fileSystem    = engineContext.GetFileSystem();

        const auto physicalPath = fileSystem.ResolvePhysical(scenePath);
        if (!physicalPath || !engineContext.GetSceneManager().LoadScene(physicalPath->string()))
        {
            LOGWARNING("Benchmark: failed to load '", scenePath,
                       "'; measuring whatever scene is currently open instead.");
        }
        else
        {
            LOGINFO("Benchmark: scene '", scenePath, "'");
        }
    }

    void EditorApplication::Cleanup()
    {
        auto& engineContext = m_Context->GetEngineContext();
        if (!engineContext.GetSettings().Save(ENGINE_SETTINGS_PATH, engineContext.GetFileSystem()))
        {
            LOGWARNING("Failed to persist engine settings on shutdown.");
        }

        m_ImGui->Shutdown(*m_Renderer);
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
