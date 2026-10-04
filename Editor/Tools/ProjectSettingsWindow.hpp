#pragma once

#include <string>
#include <vector>

namespace HedgehogEngine
{
    class EngineContext;
}

namespace Editor
{
    // File > Project Settings: edits the engine's ProjectSettings in place (name, startup scene
    // picked from assets://Scenes, game window, game data version). Save writes engine://Project.yaml
    // and points the editor's saves at the project's (possibly renamed) folder; Revert reads the
    // file again. --game-mode and a game build read the saved file.
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

        std::vector<std::string> m_Scenes;     // assets://Scenes/*.yaml, by name
        std::string              m_NameBuffer; // what the name field shows, valid or not
        std::string              m_Status;     // the last save's or revert's outcome
    };
}
