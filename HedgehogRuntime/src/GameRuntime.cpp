#include "HedgehogRuntime/api/GameRuntime.hpp"

#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/api/Containers/FontContainer.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ShadowmapingSettings.hpp"
#include "HedgehogEngine/HedgehogWindow/api/DisplayMode.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogRenderer/Renderer.hpp"
#include "HedgehogAudio/api/AudioEngine.hpp"
#include "HedgehogScripting/api/ScriptDebugger.hpp"
#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include "Logger/api/Logger.hpp"
#include "Profiler/api/Profiler.hpp"

#include <algorithm>
#include <cmath>

namespace Runtime
{
    namespace
    {
        HW::WindowMode ToWindowMode(HedgehogSettings::PlayerWindowMode mode)
        {
            using HedgehogSettings::PlayerWindowMode;
            static_assert(static_cast<int>(PlayerWindowMode::Windowed) == static_cast<int>(HW::WindowMode::Windowed));
            static_assert(static_cast<int>(PlayerWindowMode::Borderless) ==
                          static_cast<int>(HW::WindowMode::Borderless));
            static_assert(static_cast<int>(PlayerWindowMode::Fullscreen) ==
                          static_cast<int>(HW::WindowMode::Fullscreen));
            return static_cast<HW::WindowMode>(mode);
        }

        // Windowed takes the player's size, else the project's; Fullscreen the player's resolution, 0 the
        // monitor's own; Borderless none.
        void ApplyWindow(HW::Window&                              window,
                         const HedgehogSettings::PlayerSettings&  player,
                         const HedgehogSettings::ProjectSettings& project)
        {
            int width  = static_cast<int>(player.Width);
            int height = static_cast<int>(player.Height);
            if (player.WindowMode == HedgehogSettings::PlayerWindowMode::Windowed && (width == 0 || height == 0))
            {
                width  = static_cast<int>(project.GetWindowWidth());
                height = static_cast<int>(project.GetWindowHeight());
            }
            window.SetWindowMode(ToWindowMode(player.WindowMode), width, height);
            static constexpr const char* MODE_NAMES[] = { "windowed", "borderless", "fullscreen" };
            LOGINFO("[Runtime] Window: ", MODE_NAMES[static_cast<int>(player.WindowMode)], ", ", width, " x ", height,
                    ".");
        }

        RHI::PresentMode ToPresentMode(bool vsync)
        {
            return vsync ? RHI::PresentMode::VSync : RHI::PresentMode::NoVSync;
        }
    }

    GameRuntime::GameRuntime() = default;

    GameRuntime::~GameRuntime() { Shutdown(); }

    bool GameRuntime::Init(const RuntimeDesc& desc)
    {
        Profiling::SetCurrentThreadName("Main");
        if (m_Engine)
        {
            LOGERROR("[Runtime] Init: the game is already running.");
            return false;
        }
        m_Desc       = desc;
        m_FrameCount = 0;

        // The project is read before the engine, whose window it may describe, through project://
        // alone (the engine's own file system does not exist yet).
        HedgehogSettings::ProjectSettings project;
        {
            FS::FileSystemManager projectFiles;
            auto                  projectRoot = std::make_unique<FS::FileSystem>();
            (void)projectRoot->RegisterPath(FS::PROJECT_ALIAS, FS::GetProjectRootDirectory());
            (void)projectFiles.Register(std::move(projectRoot));
            if (!projectFiles.Exists(m_Desc.ProjectPath))
                LOGINFO("[Runtime] No project settings at ", m_Desc.ProjectPath, "; using the defaults.");
            else if (!project.Load(m_Desc.ProjectPath, projectFiles))
                LOGWARNING("[Runtime] Project settings could not be read, using the defaults.");
        }

        // A player's choices, read before the window they describe.
        HedgehogSettings::PlayerSettings     player = HedgehogSettings::MakeDefaultPlayerSettings(project);
        std::optional<std::filesystem::path> playerDirectory;
        if (m_Desc.UseProjectWindow)
        {
            playerDirectory = FS::GetPlayerDirectory(project.GetName(), m_Desc.EditorSaves);
            FS::FileSystemManager playerFiles;
            if (playerDirectory && FS::MountPlayer(playerFiles, *playerDirectory))
            {
                if (!playerFiles.Exists(HedgehogSettings::PLAYER_SETTINGS_PATH))
                {
                    // The defaults, written so the player has a file to edit.
                    (void)HedgehogSettings::SavePlayerSettings(playerFiles, player);
                }
                else if (HedgehogSettings::LoadPlayerSettings(playerFiles, player))
                {
                    LOGINFO("[Runtime] Player settings read from ", playerDirectory->string(), ".");
                }
            }
        }

        HedgehogEngine::WindowOptions window;
        if (m_Desc.UseProjectWindow)
        {
            window.Title  = project.GetWindowTitle().empty() ? project.GetName() : project.GetWindowTitle();
            window.Width  = static_cast<int>(project.GetWindowWidth());
            window.Height = static_cast<int>(project.GetWindowHeight());
        }
        m_Engine = std::make_unique<HedgehogEngine::Engine>(window);
        if (m_Desc.UseProjectWindow)
        {
            ApplyWindow(m_Engine->GetWindowContext().GetWindow(), player, project);
        }

        auto& engineContext = m_Engine->GetEngineContext();
        auto& fileSystem    = engineContext.GetFileSystem();
        // Scripts see no window: they choose a resolution from these (PlayerSettings.getResolutions()).
        engineContext.GetApplicationRequests().VideoModes = m_Engine->GetWindowContext().GetWindow().GetVideoModes();
        for (const RuntimeMount& mount : m_Desc.Mounts)
        {
            auto mounted = std::make_unique<FS::FileSystem>();
            if (!mounted->RegisterPath(mount.Alias, mount.Directory) || !fileSystem.Register(std::move(mounted)))
            {
                LOGERROR("[Runtime] Init: cannot mount ", mount.Directory.string(), " as ", mount.Alias, ".");
                Shutdown();
                return false;
            }
        }

        // Before the renderer is built: the settings size the GPU resources it creates.
        auto& settings = engineContext.GetSettings();
        if (fileSystem.Exists(m_Desc.SettingsPath) && !settings.Load(m_Desc.SettingsPath, fileSystem))
            LOGWARNING("[Runtime] Engine settings could not be read, using defaults.");
        settings.CleanDirtyState();
        settings.GetProjectSettings() = project;
        m_ShadowBase = HedgehogSettings::CaptureShadowQualityBase(*settings.GetShadowmapSettings());
        if (m_Desc.UseProjectWindow)
        {
            if (playerDirectory && !FS::MountPlayer(fileSystem, *playerDirectory))
            {
                LOGWARNING("[Runtime] The player folder could not be mounted as player://.");
            }
            HedgehogSettings::ApplyQualityLevel(*settings.GetShadowmapSettings(), m_ShadowBase, player.Quality);
        }
        engineContext.GetPlayerSettings() = player;
        m_AppliedPlayer                   = player;
        m_AppliedPlayerVersion            = engineContext.GetPlayerSettingsVersion();

        // Before the scene loads, as the Editor does; the engine's ECS owns it.
        const auto scripts = HedgehogScripting::RegisterScriptSystem(engineContext, fileSystem);
        // VS Code's way into the scripts, when the settings enable it: while a script is stopped
        // the window keeps handling events and its title says it is paused.
        m_ScriptDebugger = HedgehogScripting::StartScriptDebugger(engineContext, *scripts, fileSystem,
                                                                  settings.GetLuaDebuggerSettings());
        if (m_ScriptDebugger)
        {
            HW::Window& gameWindow = m_Engine->GetWindowContext().GetWindow();
            m_ScriptDebugger->SetStopCallbacks(
                [&gameWindow, title = gameWindow.GetTitle()](bool stopped)
                { gameWindow.SetTitle(stopped ? title + " - Paused in debugger" : title); },
                [&gameWindow]() { gameWindow.PollEvents(); });
        }
        (void)engineContext.GetAudioEngine().Init(HA::AudioEngineDesc{});
        if (m_Desc.UseProjectWindow)
        {
            engineContext.ApplyPlayerVolumes();
        }
        if (const auto saves = FS::GetSavesDirectory(project.GetName(), m_Desc.EditorSaves))
            engineContext.GetSaveGames().SetSaveDirectory(*saves);

        m_ScenePath = !m_Desc.ScenePath.empty()               ? m_Desc.ScenePath
                      : !project.GetStartupScene().empty() ? project.GetStartupScene()
                                                           : m_Desc.FallbackScenePath;
        const auto scenePath = fileSystem.ResolvePhysical(m_ScenePath);
        if (!scenePath || !engineContext.GetSceneManager().LoadScene(scenePath->string()))
            LOGWARNING("[Runtime] Could not load '", m_ScenePath, "'; playing an empty scene.");
        else
            LOGINFO("[Runtime] Playing ", m_ScenePath, ".");

        // A game runs its gameplay: the scene's scripts play from the first frame.
        (void)engineContext.Play();

        // Only a player's run follows the project's vsync and frame cap; --game-mode stays uncapped.
        Renderer::RendererOptions options;
        if (m_Desc.UseProjectWindow)
        {
            options.PresentMode = ToPresentMode(player.VSync);
        }
        m_MaxFps = (m_Desc.UseProjectWindow && !m_Desc.FixedFrameTime) ? player.MaxFps : 0;
        if (m_MaxFps > 0)
        {
            LOGINFO("[Runtime] Frame rate capped at ", m_MaxFps, " FPS.");
        }

        m_Renderer  = std::make_unique<Renderer::Renderer>(m_Engine->GetWindowContext().GetWindow(), fileSystem,
                                                           Renderer::DeviceReadyCallback{}, options);
        m_InputGate = HInput::GameInputGate{};
        m_LastFrame = std::chrono::steady_clock::now();
        return true;
    }

    bool GameRuntime::RunFrame()
    {
        if (!m_Engine || !m_Renderer)
            return false;
        auto& windowContext = m_Engine->GetWindowContext();
        // A script that asked to quit finished its frame; Shutdown stops Play as on window close.
        if ((m_Desc.MaxFrames > 0 && m_FrameCount >= m_Desc.MaxFrames) || windowContext.ShouldClose() ||
            m_Engine->GetEngineContext().GetApplicationRequests().QuitRequested)
            return false;

        RunFrameZones();
        // After the frame's zones have ended, so they land in the frame they measure.
        HH_PROFILE_FRAME();
        ++m_FrameCount;
        // The average leaves out the first frame, which loads and compiles what the scene needs.
        if (m_FrameCount == 1)
        {
            m_RunStart = std::chrono::steady_clock::now();
        }
        return true;
    }

    void GameRuntime::RunFrameZones()
    {
        HH_PROFILE_ZONE("Frame");
        // The cap counts from here, before input, so the period is the whole frame's.
        const std::chrono::steady_clock::time_point frameStart = std::chrono::steady_clock::now();
        auto& windowContext = m_Engine->GetWindowContext();
        auto& engineContext = m_Engine->GetEngineContext();
        auto& window        = windowContext.GetWindow();
        if (m_Desc.UseProjectWindow && engineContext.GetPlayerSettingsVersion() != m_AppliedPlayerVersion)
        {
            ApplyChangedPlayerSettings();
        }
        windowContext.HandleInput();
        if (m_ScriptDebugger)
            m_ScriptDebugger->Pump();
        // The cursor the scripts asked for; a change takes effect from this frame's input on.
        const HW::CursorMode cursor = engineContext.GetApplicationRequests().Cursor;
        if (window.GetCursorMode() != cursor)
        {
            window.SetCursorMode(cursor);
        }
        int width = 0, height = 0;
        window.GetFramebufferSize(width, height);

        // The whole window is the game view: the cursor maps from window coordinates to the
        // framebuffer's pixels, the ones the UI is laid out in.
        int windowWidth = 0, windowHeight = 0;
        window.GetWindowSize(windowWidth, windowHeight);
        HInput::GameInputRegion region;
        region.Size            = HM::Vector2(static_cast<float>(windowWidth), static_cast<float>(windowHeight));
        region.PixelSize       = HM::Vector2(static_cast<float>(width), static_cast<float>(height));
        region.PointerEnabled  = true;
        region.KeyboardEnabled = true;
        region.PointerLocked   = cursor == HW::CursorMode::Locked;
        engineContext.UpdateGameInput(HInput::MakeGameInput(window.GetRawInput(), region, m_InputGate), region.PixelSize);

        m_Engine->UpdateContext(NextFrameTime(),
                                static_cast<float>(std::max(width, 1)) / static_cast<float>(std::max(height, 1)));

        m_MeshBounds.Update(engineContext.GetResourceCatalog());
        m_RenderScene.Clear();
        m_Extractor.Extract(engineContext.GetECS(), *engineContext.GetRenderSystem(), *engineContext.GetLightSystem(),
                            *engineContext.GetCameraSystem(), m_RenderScene, m_MeshBounds.GetBounds());
        m_Extractor.ExtractUi(engineContext.GetECS(), *engineContext.GetUiSystem(), region.PixelSize, m_RenderScene,
                              &engineContext.GetResourceCatalog().GetFontContainer());
        m_Extractor.ExtractEnvironment(engineContext.GetECS(), *engineContext.GetEnvironmentSystem(), m_RenderScene);
        m_Extractor.ExtractTerrains(engineContext.GetECS(), *engineContext.GetTerrainSystem(), m_RenderScene,
                                    m_MeshBounds.GetBounds());
        m_Extractor.ExtractParticles(engineContext.GetECS(), *engineContext.GetParticleSystem(), m_RenderScene);

        m_Renderer->SyncResources(engineContext.GetResourceCatalog());
        m_Renderer->RenderFrame(m_RenderScene, engineContext.GetSettings());

        if (m_MaxFps > 0)
        {
            HH_PROFILE_ZONE("Frame cap");
            m_FrameLimiter.WaitFor(ComputeFrameWait(frameStart, std::chrono::steady_clock::now(), m_MaxFps));
        }
    }

    void GameRuntime::Run()
    {
        while (RunFrame())
        {
        }
    }

    void GameRuntime::Shutdown()
    {
        if (!m_Engine)
            return;
        if (m_FrameCount > 1)
        {
            const uint32_t frames  = m_FrameCount - 1;
            const double   seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_RunStart).count();
            LOGINFO("[Runtime] ", frames, " frames after the first in ", seconds, " s: ",
                    frames / std::max(seconds, 1e-9), " FPS on average.");
        }
        (void)m_Engine->GetEngineContext().Stop();
        // The player gets the cursor back before the window goes.
        m_Engine->GetWindowContext().GetWindow().SetCursorMode(HW::CursorMode::Normal);
        if (m_ScriptDebugger)
            m_ScriptDebugger->Pump(); // the client hears the thread exit
        m_ScriptDebugger.reset();
        if (m_Renderer)
            m_Renderer->Cleanup();
        m_Engine->Cleanup();
        m_Renderer.reset();
        m_Engine.reset();
    }

    bool GameRuntime::IsRunning() const { return m_Engine != nullptr; }

    uint32_t GameRuntime::GetFrameCount() const { return m_FrameCount; }

    const std::string& GameRuntime::GetScenePath() const { return m_ScenePath; }

    HedgehogEngine::EngineContext& GameRuntime::GetEngineContext() { return m_Engine->GetEngineContext(); }

    void GameRuntime::ApplyChangedPlayerSettings()
    {
        auto&                                   engineContext = m_Engine->GetEngineContext();
        const HedgehogSettings::PlayerSettings& player        = engineContext.GetPlayerSettings();
        const HedgehogSettings::PlayerSettings& was           = m_AppliedPlayer;
        if (player.WindowMode != was.WindowMode || player.Width != was.Width || player.Height != was.Height)
        {
            HW::Window& window = m_Engine->GetWindowContext().GetWindow();
            ApplyWindow(window, player, engineContext.GetSettings().GetProjectSettings());
            engineContext.GetApplicationRequests().VideoModes = window.GetVideoModes(); // it may be on another monitor
        }
        if (player.VSync != was.VSync)
        {
            m_Renderer->SetPresentMode(ToPresentMode(player.VSync));
        }
        if (!m_Desc.FixedFrameTime)
        {
            m_MaxFps = player.MaxFps;
        }
        if (player.MasterVolume != was.MasterVolume || player.BusVolumes != was.BusVolumes)
        {
            engineContext.ApplyPlayerVolumes();
        }
        if (player.Quality != was.Quality)
        {
            HedgehogSettings::ApplyQualityLevel(*engineContext.GetSettings().GetShadowmapSettings(), m_ShadowBase,
                                                player.Quality);
        }
        m_AppliedPlayer        = player;
        m_AppliedPlayerVersion = engineContext.GetPlayerSettingsVersion();
        LOGINFO("[Runtime] Player settings applied.");
    }

    float GameRuntime::NextFrameTime()
    {
        const auto now     = std::chrono::steady_clock::now();
        const float actual = std::chrono::duration<float>(now - m_LastFrame).count();
        m_LastFrame        = now;
        if (m_Desc.FixedFrameTime)
            return *m_Desc.FixedFrameTime;
        return std::isfinite(actual) ? std::clamp(actual, 0.0f, MAX_FRAME_TIME) : 0.0f;
    }
}
