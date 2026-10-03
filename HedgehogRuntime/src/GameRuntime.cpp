#include "HedgehogRuntime/api/GameRuntime.hpp"

#include "HedgehogEngine/api/Engine.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/WindowContext.hpp"
#include "HedgehogEngine/api/Containers/FontContainer.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogRenderer/Renderer.hpp"
#include "HedgehogAudio/api/AudioEngine.hpp"
#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cmath>

namespace Runtime
{
    GameRuntime::GameRuntime() = default;

    GameRuntime::~GameRuntime() { Shutdown(); }

    bool GameRuntime::Init(const RuntimeDesc& desc)
    {
        if (m_Engine)
        {
            LOGERROR("[Runtime] Init: the game is already running.");
            return false;
        }
        m_Desc       = desc;
        m_FrameCount = 0;
        m_Engine     = std::make_unique<HedgehogEngine::Engine>();

        auto& engineContext = m_Engine->GetEngineContext();
        auto& fileSystem    = engineContext.GetFileSystem();
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

        // Before the scene loads, as the Editor does; the engine's ECS owns it.
        (void)HedgehogScripting::RegisterScriptSystem(engineContext, fileSystem);
        (void)engineContext.GetAudioEngine().Init(HA::AudioEngineDesc{});
        if (const auto saves = FS::GetSavesDirectory(m_Desc.ProjectName, m_Desc.EditorSaves))
            engineContext.GetSaveGames().SetSaveDirectory(*saves);

        // Before the renderer is built: the settings size the GPU resources it creates.
        auto& settings = engineContext.GetSettings();
        if (fileSystem.Exists(m_Desc.SettingsPath) && !settings.Load(m_Desc.SettingsPath, fileSystem))
            LOGWARNING("[Runtime] Engine settings could not be read, using defaults.");
        settings.CleanDirtyState();

        const auto scenePath = fileSystem.ResolvePhysical(m_Desc.ScenePath);
        if (!scenePath || !engineContext.GetSceneManager().LoadScene(scenePath->string()))
            LOGWARNING("[Runtime] Could not load '", m_Desc.ScenePath, "'; playing an empty scene.");

        // A game runs its gameplay: the scene's scripts play from the first frame.
        (void)engineContext.Play();

        m_Renderer  = std::make_unique<Renderer::Renderer>(m_Engine->GetWindowContext().GetWindow(), fileSystem);
        m_InputGate = HInput::GameInputGate{};
        m_LastFrame = std::chrono::steady_clock::now();
        return true;
    }

    bool GameRuntime::RunFrame()
    {
        if (!m_Engine || !m_Renderer)
            return false;
        auto& windowContext = m_Engine->GetWindowContext();
        if ((m_Desc.MaxFrames > 0 && m_FrameCount >= m_Desc.MaxFrames) || windowContext.ShouldClose())
            return false;

        auto& engineContext = m_Engine->GetEngineContext();
        auto& window        = windowContext.GetWindow();
        windowContext.HandleInput();
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
        engineContext.UpdateGameInput(HInput::MakeGameInput(window.GetRawInput(), region, m_InputGate), region.PixelSize);

        m_Engine->UpdateContext(NextFrameTime(),
                                static_cast<float>(std::max(width, 1)) / static_cast<float>(std::max(height, 1)));

        m_MeshBounds.Update(engineContext.GetResourceCatalog());
        m_RenderScene.Clear();
        m_Extractor.Extract(engineContext.GetECS(), *engineContext.GetRenderSystem(), *engineContext.GetLightSystem(),
                            *engineContext.GetCameraSystem(), m_RenderScene, m_MeshBounds.GetBounds());
        m_Extractor.ExtractUi(engineContext.GetECS(), *engineContext.GetUiSystem(), region.PixelSize, m_RenderScene,
                              &engineContext.GetResourceCatalog().GetFontContainer());

        m_Renderer->SyncResources(engineContext.GetResourceCatalog());
        m_Renderer->RenderFrame(m_RenderScene, engineContext.GetSettings());
        ++m_FrameCount;
        return true;
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
        (void)m_Engine->GetEngineContext().Stop();
        if (m_Renderer)
            m_Renderer->Cleanup();
        m_Engine->Cleanup();
        m_Renderer.reset();
        m_Engine.reset();
    }

    bool GameRuntime::IsRunning() const { return m_Engine != nullptr; }

    uint32_t GameRuntime::GetFrameCount() const { return m_FrameCount; }

    HedgehogEngine::EngineContext& GameRuntime::GetEngineContext() { return m_Engine->GetEngineContext(); }

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
