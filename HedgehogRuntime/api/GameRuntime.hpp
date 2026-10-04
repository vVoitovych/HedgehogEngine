#pragma once

#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

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
        // The scene to play (virtual, e.g. engine://Assets/Scenes/Hud.yaml). Empty plays the
        // project's startup scene, or FallbackScenePath when the project names none.
        std::string                 ScenePath;
        std::string                 FallbackScenePath = "assets://Scenes/Default.yaml";
        uint32_t                    MaxFrames = 0;                  // 0: until the window closes
        std::optional<float>        FixedFrameTime;                 // every frame's dt; nullopt measures real time
        std::string                 SettingsPath = "engine://engine_settings.yaml"; // read, never written
        std::string                 ProjectPath  = HedgehogSettings::ProjectSettings::PATH; // under engine://; read, never written
        bool                        UseProjectWindow = false;       // the project's title, size and fullscreen; else 1366x768
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

        // Builds everything and starts Play. The project settings name the saves folder (by the
        // project's name), the game data version and, unless the desc names a scene, the scene. A
        // scene that does not load is logged and an empty one plays. False, logged, when already
        // running or a mount cannot be registered.
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

        // The scene Init played (after the project's startup scene and the fallback were applied).
        [[nodiscard]] const std::string& GetScenePath() const;

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

        std::string                               m_ScenePath;
        uint32_t                                  m_FrameCount = 0;
        std::chrono::steady_clock::time_point     m_LastFrame;
    };
}
