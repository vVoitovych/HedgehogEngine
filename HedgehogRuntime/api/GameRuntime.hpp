#pragma once

#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "HedgehogExtract/api/MeshBounds.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/SceneExtractor.hpp"
#include "HedgehogInput/api/GameInputRegion.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace HedgehogEngine
{
    class Engine;
    class EngineContext;
}

namespace Renderer
{
    class Renderer;
}

namespace Runtime
{
    // A folder mounted into the engine's file system before anything loads ("game://").
    struct RuntimeMount
    {
        std::string           Alias;
        std::filesystem::path Directory;
    };

    // What a game run needs: which scene, for how long, at what step, and where things live.
    struct RuntimeDesc
    {
        std::string                 ScenePath;                      // virtual, e.g. engine://Assets/Scenes/Default.yaml
        uint32_t                    MaxFrames = 0;                  // 0: until the window closes
        std::optional<float>        FixedFrameTime;                 // every frame's dt; nullopt measures real time
        std::string                 SettingsPath = "engine://engine_settings.yaml"; // read, never written
        std::string                 ProjectName  = HedgehogEngine::SaveGameManager::DEFAULT_PROJECT_NAME;
        bool                        EditorSaves  = false;           // the project's editor saves folder, not the game's
        std::vector<RuntimeMount>   Mounts;
    };

    // The editor-free game loop: an engine and its window, the script system, audio, saves,
    // settings, the scene in Play, and a renderer drawing the scene's cameras each frame through
    // the render graph. Creates no ImGui context and includes no ImGui (CheckModuleBoundaries.ps1).
    // The Editor's --game-mode drives it frame by frame; a Game executable will call Run.
    class GameRuntime
    {
    public:
        // Real-time frames longer than this count as this long, so a stall never floods the clock.
        static constexpr float MAX_FRAME_TIME = 0.25f;

        GameRuntime();
        ~GameRuntime();

        GameRuntime(const GameRuntime&)            = delete;
        GameRuntime& operator=(const GameRuntime&) = delete;
        GameRuntime(GameRuntime&&)                 = delete;
        GameRuntime& operator=(GameRuntime&&)      = delete;

        // Builds everything and starts Play. A scene that does not load is logged and an empty one
        // plays. False, logged, when already running or a mount cannot be registered.
        bool Init(const RuntimeDesc& desc);

        // One frame: input, simulation, extraction, render. False, rendering nothing, once the
        // frame limit is reached, the window is closing, or before Init.
        bool RunFrame();

        // RunFrame until it returns false.
        void Run();

        // Stops Play and tears the renderer and the engine down; safe twice and before Init.
        void Shutdown();

        [[nodiscard]] bool     IsRunning() const;
        [[nodiscard]] uint32_t GetFrameCount() const;

        // Valid between Init and Shutdown.
        [[nodiscard]] HedgehogEngine::EngineContext& GetEngineContext();

    private:
        float NextFrameTime();

    private:
        RuntimeDesc                               m_Desc;
        std::unique_ptr<HedgehogEngine::Engine>   m_Engine;
        std::unique_ptr<Renderer::Renderer>       m_Renderer; // declared after the engine: destroyed first

        HX::RenderScene                           m_RenderScene;
        HX::MeshBoundsCache                       m_MeshBounds;
        HX::SceneExtractor                        m_Extractor;
        HInput::GameInputGate                     m_InputGate;

        uint32_t                                  m_FrameCount = 0;
        std::chrono::steady_clock::time_point     m_LastFrame;
    };
}
