#pragma once

#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include <optional>
#include <string>
#include <vector>

namespace HedgehogEngine
{
    class EngineContext;
}

namespace Editor
{
    // File > Project Settings: edits the engine's ProjectSettings in place (name, startup scene
    // picked from assets://Scenes, game window, game data version, plugins). Save writes
    // engine://Project.yaml, points the editor's saves at the project's (possibly renamed) folder
    // and makes the loaded plugins the saved list's enabled ones (in Play, once Play stops); Revert
    // reads the file again. --game-mode and a game build read the saved file.
    class ProjectSettingsWindow
    {
    public:
        bool Open = false;

        // Shows the window with the scene list read again.
        void Show(const HedgehogEngine::EngineContext& context);

        void Draw(HedgehogEngine::EngineContext& context);

    private:
        void Refresh(const HedgehogEngine::EngineContext& context);
        void Save(HedgehogEngine::EngineContext& context);
        void Revert(HedgehogEngine::EngineContext& context);
        void DrawPlugins(HedgehogEngine::EngineContext& context);
        void ApplyPlugins(HedgehogEngine::EngineContext& context, const HedgehogSettings::ProjectSettings& saved);

        std::vector<std::string> m_Scenes;     // assets://Scenes/*.yaml, by name
        std::string              m_NameBuffer; // what the name field shows, valid or not
        std::string              m_Status;     // the last save's or revert's outcome
        std::string              m_PluginNameBuffer; // the Add field
        // The project as saved during Play, whose plugins apply once Play stops.
        std::optional<HedgehogSettings::ProjectSettings> m_PendingPlugins;
    };
}
