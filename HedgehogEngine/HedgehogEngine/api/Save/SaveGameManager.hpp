#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"

#include "EcsSerialization/api/SaveGame/SaveSlotStore.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

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
    class SaveGameManager
    {
    public:
        using SaveSection = std::function<YAML::Node()>;
        using LoadSection = std::function<void(const YAML::Node&)>;

        // Until project settings exist, every project saves under this name.
        static constexpr const char* DEFAULT_PROJECT_NAME = "HedgehogEngine";

        HEDGEHOG_ENGINE_API SaveGameManager(SceneManager& scenes, EventBus& eventBus, const FixedStepClock& clock);

        // Where the slots live: the Editor's per-project editor folder, --game-mode's game folder,
        // a test's temp folder. Until set, every request fails with an error.
        HEDGEHOG_ENGINE_API void SetSaveDirectory(const std::filesystem::path& directory);

        // Written into each save's metadata (the game's own data version).
        HEDGEHOG_ENGINE_API void SetGameDataVersion(int version);

        // A section saved and loaded with the world; a name registered again replaces it.
        HEDGEHOG_ENGINE_API void RegisterSection(const std::string& name, SaveSection save, LoadSection load);
        HEDGEHOG_ENGINE_API void UnregisterSection(const std::string& name);

        // Queue a save of the current world into slot, or a load of slot, for the end of the
        // frame. False, logged, for an invalid slot name, no save directory, or (load) a slot
        // that does not exist.
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

        SceneManager&          m_Scenes;
        EventBus&              m_EventBus;
        const FixedStepClock&  m_Clock;

        std::optional<EcsSerialization::SaveSlotStore> m_Store;
        int                                            m_GameDataVersion = 1;
        std::vector<Section>                           m_Sections;
        std::vector<std::string>                       m_PendingSaves;
        std::optional<std::string>                     m_PendingLoad;
    };
}
