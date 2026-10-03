#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/SaveEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Time/FixedStepClock.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <utility>

namespace HedgehogEngine
{
    namespace
    {
        // Now, as ISO 8601 UTC: 2026-10-03T21:30:00Z.
        std::string CurrentTimestamp()
        {
            const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm           utc{};
            gmtime_s(&utc, &now);
            char text[32] = {};
            std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%SZ", &utc);
            return text;
        }
    }

    SaveGameManager::SaveGameManager(SceneManager& scenes, EventBus& eventBus, const FixedStepClock& clock)
        : m_Scenes(scenes)
        , m_EventBus(eventBus)
        , m_Clock(clock)
    {
    }

    void SaveGameManager::SetSaveDirectory(const std::filesystem::path& directory) { m_Store.emplace(directory); }

    void SaveGameManager::SetGameDataVersion(int version) { m_GameDataVersion = version; }

    void SaveGameManager::RegisterSection(const std::string& name, SaveSection save, LoadSection load)
    {
        UnregisterSection(name);
        m_Sections.push_back({ name, std::move(save), std::move(load) });
    }

    void SaveGameManager::UnregisterSection(const std::string& name)
    {
        std::erase_if(m_Sections, [&name](const Section& section) { return section.Name == name; });
    }

    bool SaveGameManager::RequestSave(const std::string& slot)
    {
        if (!m_Store)
        {
            LOGERROR("[Save] No save directory is set; slot '" + slot + "' is not saved.");
            return false;
        }
        if (!EcsSerialization::SaveSlotStore::IsValidSlotName(slot))
        {
            LOGERROR("[Save] '" + slot + "' is not a slot name (1 to 64 of A-Z, a-z, 0-9, '_' and '-').");
            return false;
        }
        m_PendingSaves.push_back(slot);
        return true;
    }

    bool SaveGameManager::RequestLoad(const std::string& slot)
    {
        if (!m_Store)
        {
            LOGERROR("[Save] No save directory is set; slot '" + slot + "' is not loaded.");
            return false;
        }
        if (!m_Store->Exists(slot)) // logs an invalid name itself
        {
            if (EcsSerialization::SaveSlotStore::IsValidSlotName(slot))
                LOGERROR("[Save] Slot '" + slot + "' does not exist.");
            return false;
        }
        m_PendingLoad = slot;
        return true;
    }

    std::vector<EcsSerialization::SaveSlotInfo> SaveGameManager::ListSlots() const
    {
        return m_Store ? m_Store->List() : std::vector<EcsSerialization::SaveSlotInfo>{};
    }

    bool SaveGameManager::SlotExists(const std::string& slot) const { return m_Store && m_Store->Exists(slot); }

    bool SaveGameManager::DeleteSlot(const std::string& slot) const { return m_Store && m_Store->Delete(slot); }

    bool SaveGameManager::IsValidSlotName(const std::string& slot)
    {
        return EcsSerialization::SaveSlotStore::IsValidSlotName(slot);
    }

    void SaveGameManager::ProcessRequests()
    {
        for (const std::string& slot : std::exchange(m_PendingSaves, {}))
            Save(slot);
        if (std::optional<std::string> slot = std::exchange(m_PendingLoad, std::nullopt))
            Load(*slot);
    }

    void SaveGameManager::ClearRequests()
    {
        m_PendingSaves.clear();
        m_PendingLoad.reset();
    }

    void SaveGameManager::Save(const std::string& slot)
    {
        EcsSerialization::SaveGameFile save;
        save.Metadata.GameDataVersion = m_GameDataVersion;
        save.Metadata.ScenePath       = m_Scenes.GetScenePath();
        save.Metadata.Timestamp       = CurrentTimestamp();
        save.Metadata.PlayTime        = m_Clock.Time;
        save.Sections[EcsSerialization::SAVE_SECTION_WORLD] = m_Scenes.CaptureWorld();
        for (const Section& section : m_Sections)
            save.Sections[section.Name] = section.Save();

        if (m_Store->Write(slot, save))
            LOGINFO("[Save] Saved slot '" + slot + "'.");
    }

    void SaveGameManager::Load(const std::string& slot)
    {
        const std::optional<EcsSerialization::SaveGameFile> save = m_Store->Read(slot); // logs why it failed
        if (!save)
            return;
        const auto world = save->Sections.find(EcsSerialization::SAVE_SECTION_WORLD);
        if (world == save->Sections.end())
        {
            LOGERROR("[Save] Slot '" + slot + "' has no World section; it is not loaded.");
            return;
        }

        if (!m_Scenes.RestoreWorld(world->second, "the World of save slot '" + slot + "'"))
            LOGERROR("[Save] Slot '" + slot + "': the world could not be restored completely.");
        for (const Section& section : m_Sections)
        {
            if (const auto data = save->Sections.find(section.Name); data != save->Sections.end())
                section.Load(data->second);
        }
        LOGINFO("[Save] Loaded slot '" + slot + "'.");
        m_EventBus.Publish(GameLoadedEvent{ slot });
    }
}
