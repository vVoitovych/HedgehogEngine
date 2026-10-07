#include "Application.hpp"
#include "EditorGui.hpp"
#include "ImGuiLayer.hpp"

#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"

#include "HedgehogAudio/api/AudioEngine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/UiEvents.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/LayerSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"
#include "HedgehogEngine/api/Plugins/PluginManager.hpp"
#include "HedgehogCommon/api/Camera.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"
#include "HedgehogEngine/api/Containers/FontContainer.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"
#include "HedgehogExtract/api/ScenePicker.hpp"
#include "HedgehogRenderer/Renderer.hpp"
#include "HedgehogScripting/api/ScriptDebugger.hpp"
#include "HedgehogScripting/api/ScriptSystem.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "ECS/api/components/Hierarchy.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include "Logger/api/Logger.hpp"

#include "Project/StartupProject.hpp"

#include "imgui.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <numeric>
#include <vector>

namespace Editor
{
    namespace
    {
        constexpr const char* ENGINE_SETTINGS_PATH = HedgehogSettings::Settings::PATH;

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
            desc.GraphName = SCENE_GRAPH;
            desc.Priority  = 100;
            return desc;
        }

        // The editor's per-user folder, <LocalAppData>/HedgehogEngine/Editor (a temp folder when
        // LocalAppData is not set).
        std::filesystem::path GetEditorUserFolder()
        {
            return FS::GetEditorUserDirectory().value_or(std::filesystem::temp_directory_path() / "HedgehogEngine" /
                                                         "Editor");
        }

        // The editor's UI into the window. It has no camera, and it reads both panels' targets, so
        // it is ordered after the views that write them.
        Renderer::ViewDesc MakeResultView()
        {
            Renderer::ViewDesc desc;
            desc.Targets   = { std::string(Renderer::RenderTargetRegistry::MAIN_TARGET) };
            desc.Reads     = { SCENE_TARGET, GAME_TARGET };
            desc.GraphName = RESULT_GRAPH;
            desc.Priority  = 100;
            return desc;
        }

    }

    bool SelectStartupProject(const std::string& projectArgument, bool useRecentProjects)
    {
        // The recent list, read from the per-user settings before the engine (and its file system)
        // exists; a missing or unreadable file is an empty list.
        EditorSettings settings;
        if (useRecentProjects && projectArgument.empty())
        {
            FS::FileSystemManager userFiles;
            auto                  folder = std::make_unique<FS::FileSystem>();
            if (folder->RegisterPath(EditorSettings::USER_ALIAS, GetEditorUserFolder()) &&
                userFiles.Register(std::move(folder)) && userFiles.Exists(EditorSettings::PATH))
                (void)settings.Load(EditorSettings::PATH, userFiles);
        }

        // With no override yet, the project root is the default: the dev tree's sample, or a
        // package's own folder.
        FS::SetProjectRootDirectory({});
        const StartupProjectChoice choice =
            ChooseStartupProject(projectArgument, settings.RecentProjects, FS::GetProjectRootDirectory());
        if (!choice.Error.empty())
        {
            LOGERROR("[Editor] ", choice.Error);
            return false;
        }
        FS::SetProjectRootDirectory(choice.Path);
        LOGINFO("[Editor] Opening the project at ", choice.Path.string(), ".");
        return true;
    }

    EditorApplication::EditorApplication(HedgehogEngine::WindowMode windowMode, bool recordRecentProject)
        : m_WindowMode(windowMode)
        , m_RecordRecentProject(recordRecentProject)
    {
    }

    EditorApplication::~EditorApplication() = default;

    RunResult EditorApplication::Run(uint32_t maxFrames)
    {
        m_Result = {};
        Init();
        MainLoop(maxFrames);
        return m_Result;
    }

    void EditorApplication::Init()
    {
        m_Context   = std::make_unique<HedgehogEngine::Engine>(m_WindowMode);

        auto& engineContext = m_Context->GetEngineContext();

        // The editor's personal state (its layout, imgui.ini, the recent projects) is per user and
        // outside any project: user:// at <LocalAppData>/HedgehogEngine/Editor, created if missing.
        const std::filesystem::path userDirectory = GetEditorUserFolder();
        std::error_code userError;
        std::filesystem::create_directories(userDirectory, userError);
        if (userError)
            LOGERROR("The editor's folder ", userDirectory.string(), " cannot be created: ", userError.message());
        auto userFiles = std::make_unique<FS::FileSystem>();
        if (!userFiles->RegisterPath(EditorSettings::USER_ALIAS, userDirectory) ||
            !engineContext.GetFileSystem().Register(std::move(userFiles)))
            LOGERROR("The editor's folder ", userDirectory.string(), " cannot be mounted as ", EditorSettings::USER_ALIAS);

        // After the engine's own systems, so its play-mode events come after theirs, and before
        // EditorGui loads the last scene. The engine's ECS owns it; the editor only points at it.
        m_ScriptSystem = HedgehogScripting::RegisterScriptSystem(engineContext, engineContext.GetFileSystem()).get();

        // A click on a game UI button (Play mode only) shows in the Console.
        engineContext.GetEventBus().Subscribe<HedgehogEngine::UiButtonClickedEvent>(
            [&ecs = engineContext.GetECS()](const HedgehogEngine::UiButtonClickedEvent& event)
            {
                const bool named = ecs.IsAlive(event.Entity) && ecs.HasComponent<ECS::HierarchyComponent>(event.Entity);
                LOGINFO("[UI] Button clicked:", named ? ecs.GetComponent<ECS::HierarchyComponent>(event.Entity).Name
                                                      : std::string("<unnamed>"),
                        "( entity", event.Entity, ")");
            });

        // The project names the saves folder and the game data version; a missing file keeps the
        // defaults until File > Project Settings saves one.
        auto& project = engineContext.GetSettings().GetProjectSettings();
        if (engineContext.GetFileSystem().Exists(HedgehogSettings::ProjectSettings::PATH) &&
            !project.Load(HedgehogSettings::ProjectSettings::PATH, engineContext.GetFileSystem()))
            LOGWARNING("Project settings could not be read, using the defaults.");
        // The window names the open project.
        m_Context->GetWindowContext().GetWindow().SetTitle("HedgehogEngine - " + project.GetName());

        // The project's enabled plugins, after the script system and before EditorGui loads the
        // last scene, so their components read; one that fails is logged and the rest load. Shadow
        // copies leave each plugin's own DLL free to be rebuilt while the editor runs.
        engineContext.GetPlugins().SetShadowCopy(true);
        (void)engineContext.GetPlugins().ApplyProjectPlugins(project);

        // Play sessions save into the project's editor folder, so they never overwrite a game's saves.
        if (const auto saves = FS::GetSavesDirectory(project.GetName(), true))
            engineContext.GetSaveGames().SetSaveDirectory(*saves);

        // Sounds play only in Play mode; a missing output device leaves the engine silent.
        (void)engineContext.GetAudioEngine().Init(HA::AudioEngineDesc{});

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

        // VS Code's way into the scripts, when the settings enable it. While a script is stopped
        // the editor's frame waits inside it: the window keeps handling events and its title says
        // why it does not redraw.
        m_ScriptDebugger = HedgehogScripting::StartScriptDebugger(engineContext, *m_ScriptSystem, fileSystem,
                                                                  settings.GetLuaDebuggerSettings());
        if (m_ScriptDebugger)
        {
            HW::Window& window = m_Context->GetWindowContext().GetWindow();
            m_ScriptDebugger->SetStopCallbacks(
                [&window, title = window.GetTitle()](bool stopped) { window.SetTitle(stopped ? title + " - Paused in debugger" : title); },
                [&window]() { window.PollEvents(); });
        }

        // ImGui's context first: the renderer hands it the device to build its GUI renderer on.
        m_ImGui    = std::make_unique<ImGuiLayer>(m_Context->GetWindowContext().GetWindow(),
                                                  userDirectory / EditorSettings::IMGUI_INI);
        m_ImGui->LoadFonts(fileSystem);
        m_Renderer = std::make_unique<Renderer::Renderer>(
            m_Context->GetWindowContext().GetWindow(), engineContext.GetFileSystem(),
            [this, &fileSystem](const Renderer::RendererDevice& device)
            {
                m_ImGui->CreateBackend(device);
                m_ContentIcons.Load(device.Device, fileSystem);
                m_EditorIcons.Load(device.Device, fileSystem);
            });
        // The icons never change, so neither do their ids.
        for (size_t index = 0; index < CONTENT_TYPE_COUNT; ++index)
        {
            const ContentType type = static_cast<ContentType>(index);
            m_ContentIconIds[index] = m_ImGui->GetTextureId(std::string("icon:") + GetContentTypeName(type),
                                                            m_ContentIcons.Get(type));
        }
        for (size_t index = 0; index < EDITOR_ICON_COUNT; ++index)
        {
            const EditorIcon icon = static_cast<EditorIcon>(index);
            m_EditorIconIds[index] = m_ImGui->GetTextureId(std::string("icon:ui:") + GetEditorIconFile(icon),
                                                           m_EditorIcons.Get(icon));
        }
        m_EditorGui = std::make_unique<EditorGui>(*m_Context, m_RecordRecentProject);
        m_EditorGui->SetRenderer(m_Renderer.get());
        m_EditorGui->SetScriptSystem(m_ScriptSystem);
        m_EditorGui->SetMonoFont(m_ImGui->GetMonoFont());

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

        LOGINFO("Editor initialized");
    }

    void EditorApplication::RunBenchmark(uint32_t warmupFrames, uint32_t measureFrames, const std::string& sceneFile)
    {
        Init();
        m_EditorGui->SetBenchmarkMode(true);
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
            // Another project: this editor is torn down as on exit and the caller builds one on it.
            if ((m_Result.RequestedProject = m_EditorGui->TakeProjectRequest()))
                break;
        }

        Cleanup();
    }

    float EditorApplication::StepFrame()
    {
        const float dt = GetFrameTime();
        m_Context->GetWindowContext().HandleInput();
        // The game sees the Game tab's part of the window's input, the editor camera the Scene tab's
        // (both panels as of the last frame).
        const HW::RawInput& raw = m_Context->GetWindowContext().GetWindow().GetRawInput();
        auto&               engineContext = m_Context->GetEngineContext();
        const HInput::GameInputRegion gameRegion = m_EditorGui->GetGameInputRegion();
        engineContext.UpdateGameInput(HInput::MakeGameInput(raw, gameRegion, m_GameInputGate), gameRegion.PixelSize);
        engineContext.UpdateEditorInput(HInput::MakeGameInput(raw, m_EditorGui->GetSceneInputRegion(), m_SceneInputGate));
        if (m_ScriptDebugger)
            m_ScriptDebugger->Pump();
        // A script saved on disk takes effect without leaving Play (polled at most once a second).
        m_ScriptSystem->ReloadChangedScripts(m_Context->GetEngineContext().GetECS());
        // A rebuilt plugin DLL is reloaded in place, in Edit mode (polled at most once a second).
        (void)m_Context->GetEngineContext().GetPlugins().ReloadChangedPlugins(std::chrono::steady_clock::now());
        m_Context->UpdateContext(dt, GetSceneAspectRatio());

        m_ImGui->BeginFrame();
        ViewportImages images;
        images.Scene          = m_ImGui->GetTextureId(SCENE_TARGET, m_Renderer->GetTargetTexture(SCENE_TARGET));
        images.Game           = m_ImGui->GetTextureId(GAME_TARGET, m_Renderer->GetTargetTexture(GAME_TARGET));
        images.GraphPassCount = m_Renderer->GetLastFramePassCount();
        images.ContentIcons   = m_ContentIconIds;
        images.EditorIcons    = m_EditorIconIds;
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
        // The game UI, laid out for the game panel the game view draws into.
        HX::SceneExtractor{}.ExtractUi(engineContext.GetECS(), *engineContext.GetUiSystem(),
                                       HM::Vector2(static_cast<float>(m_EditorGui->GetGameViewWidth()),
                                                   static_cast<float>(m_EditorGui->GetGameViewHeight())),
                                       m_RenderScene, &engineContext.GetResourceCatalog().GetFontContainer());

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
        m_ScriptDebugger.reset(); // unhooks the script system's Lua state while it still exists
        auto& engineContext = m_Context->GetEngineContext();
        if (!engineContext.GetSettings().Save(ENGINE_SETTINGS_PATH, engineContext.GetFileSystem()))
        {
            LOGWARNING("Failed to persist engine settings on shutdown.");
        }

        m_ImGui->Shutdown();
        m_ContentIcons.Release();
        m_EditorIcons.Release();
        m_Renderer->Cleanup();
        m_Context->Cleanup();
    }

    // The flycam's aspect: its scene panel's, or the window's while the panel has no size yet.
    float EditorApplication::GetSceneAspectRatio() const
    {
        uint32_t width  = m_EditorGui->GetSceneViewWidth();
        uint32_t height = m_EditorGui->GetSceneViewHeight();
        if (width == 0 || height == 0)
        {
            int windowWidth = 0, windowHeight = 0;
            m_Context->GetWindowContext().GetWindow().GetFramebufferSize(windowWidth, windowHeight);
            width  = static_cast<uint32_t>(std::max(windowWidth, 1));
            height = static_cast<uint32_t>(std::max(windowHeight, 1));
        }
        return static_cast<float>(width) / static_cast<float>(height);
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
