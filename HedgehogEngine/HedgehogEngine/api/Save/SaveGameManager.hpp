#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "EcsSerialization/api/SaveGame/SaveMigration.hpp"
#include "EcsSerialization/api/SaveGame/SaveSlotStore.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace HedgehogSettings
{
    class Settings;
}

namespace HedgehogEngine
{
    class EventBus;
    class SceneManager;
    struct FixedStepClock;

    // Save games in Play mode (epic HE-167): a save holds the world (SceneManager's tree, section
    // "World") and every section another module registers (the script system's "Scripts"), so
    // the engine never depends on what is above it.
    //
    // Requests wait for the end of the frame's gameplay: EngineContext::UpdatePlayMode calls
    // ProcessRequests after the fixed steps and the update, so a save sees a consistent state and
    // a load never tears the world down under a running script. Saves run first, in request order;
    // then the last load requested, which replaces the whole world with the saved one (the world
    // the save was made in, whatever scene is open), hands each registered section its data and
    // publishes GameLoadedEvent. Stop drops pending requests, and the editor's Stop still restores
    // the scene from before Play, so a load in Play never reaches the edited scene.
    //
    // Versions: every save carries the save format (SAVE_FORMAT_VERSION) and the game's data
    // version (the project settings' GetGameDataVersion). A save newer than either is refused,
    // naming the versions. An older game data version is migrated on load: the registered C++
    // steps run in order on the save's sections before the world is touched (a failing step
    // refuses the load), then each section's loader gets the saved version (the script system
    // calls each script's OnMigrate(fromVersion, state) before its OnLoad).
    class SaveGameManager
    {
    public:
        using SaveSection = std::function<YAML::Node()>;
        // The section's data and the game data version it was saved at (already migrated by the
        // C++ steps, so the data reads as the current version's unless a module migrates its own).
        using LoadSection = std::function<void(const YAML::Node&, int savedGameDataVersion)>;

        HEDGEHOG_ENGINE_API SaveGameManager(SceneManager& scenes, EventBus& eventBus, const FixedStepClock& clock,
                                            const HedgehogSettings::Settings& settings);

        // Where the slots live: the Editor's per-project editor folder, --game-mode's game folder,
        // a test's temp folder. Until set, every request fails with an error.
        HEDGEHOG_ENGINE_API void SetSaveDirectory(const std::filesystem::path& directory);

        // The game data version saves are written at and migrated to: the project settings'
        // (Project.yaml's game_data_version), read on every call, so an edit applies to the next save.
        [[nodiscard]] HEDGEHOG_ENGINE_API int GetGameDataVersion() const;

        // The C++ step migrating a save's sections from game data version fromVersion to
        // fromVersion + 1 (see EcsSerialization::SaveMigrationRegistry); a version may have none.
        HEDGEHOG_ENGINE_API void RegisterMigration(int fromVersion, EcsSerialization::SaveMigrationStep step);

        // Why a save with this header cannot load in this build ("the save is game data version 3,
        // but this game is version 2"), or empty when it can.
        [[nodiscard]] HEDGEHOG_ENGINE_API std::string GetUnloadableReason(const EcsSerialization::SaveGameMetadata& metadata) const;

        // A section saved and loaded with the world; a name registered again replaces it.
        HEDGEHOG_ENGINE_API void RegisterSection(const std::string& name, SaveSection save, LoadSection load);
        HEDGEHOG_ENGINE_API void UnregisterSection(const std::string& name);

        // Queue a save of the current world into slot, or a load of slot, for the end of the
        // frame. False, logged, for an invalid slot name, no save directory, or (load) a slot
        // that does not exist or cannot load (GetUnloadableReason).
        HEDGEHOG_ENGINE_API bool RequestSave(const std::string& slot);
        HEDGEHOG_ENGINE_API bool RequestLoad(const std::string& slot);

        // The store's slots (empty, false without a save directory); invalid names log and fail.
        [[nodiscard]] HEDGEHOG_ENGINE_API std::vector<EcsSerialization::SaveSlotInfo> ListSlots() const;
        [[nodiscard]] HEDGEHOG_ENGINE_API bool SlotExists(const std::string& slot) const;
        HEDGEHOG_ENGINE_API bool DeleteSlot(const std::string& slot) const;
        [[nodiscard]] HEDGEHOG_ENGINE_API static bool IsValidSlotName(const std::string& slot);

        HEDGEHOG_ENGINE_API void ProcessRequests();
        HEDGEHOG_ENGINE_API void ClearRequests();

    private:
        void Save(const std::string& slot);
        void Load(const std::string& slot);

    private:
        struct Section
        {
            std::string Name;
            SaveSection Save;
            LoadSection Load;
        };

        SceneManager&                     m_Scenes;
        EventBus&                         m_EventBus;
        const FixedStepClock&             m_Clock;
        const HedgehogSettings::Settings& m_Settings;

        std::optional<EcsSerialization::SaveSlotStore> m_Store;
        EcsSerialization::SaveMigrationRegistry        m_Migrations;
        std::vector<Section>                           m_Sections;
        std::vector<std::string>                       m_PendingSaves;
        std::optional<std::string>                     m_PendingLoad;
    };
}
