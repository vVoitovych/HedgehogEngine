#include "EcsSerialization/api/SaveGame/SaveSlotStore.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

namespace EcsSerialization
{
    namespace
    {
        std::optional<std::string> ReadText(const std::filesystem::path& path)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return std::nullopt;
            std::ostringstream text;
            text << file.rdbuf();
            return text.str();
        }
    }

    SaveSlotStore::SaveSlotStore(std::filesystem::path directory)
        : m_Directory(std::move(directory))
    {
    }

    bool SaveSlotStore::IsValidSlotName(std::string_view slot)
    {
        return !slot.empty() && slot.size() <= MAX_SLOT_NAME &&
               std::all_of(slot.begin(), slot.end(),
                           [](char c)
                           {
                               return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                                      c == '_' || c == '-';
                           });
    }

    bool SaveSlotStore::Write(const std::string& slot, const SaveGameFile& save) const
    {
        const std::optional<std::filesystem::path> path = SlotPath(slot, "Write");
        if (!path)
            return false;

        std::error_code error;
        std::filesystem::create_directories(m_Directory, error);
        if (error)
        {
            LOGERROR("[Save] Cannot create " + m_Directory.string() + ": " + error.message());
            return false;
        }

        std::filesystem::path temp = *path;
        temp.replace_extension(TEMP_EXTENSION);
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file << WriteSaveGame(save);
            file.close();
            if (!file)
            {
                LOGERROR("[Save] Slot '" + slot + "': cannot write " + temp.string() + ".");
                std::filesystem::remove(temp, error);
                return false;
            }
        }

        // Replaces the previous save in one step: until here it is untouched.
        std::filesystem::rename(temp, *path, error);
        if (error)
        {
            LOGERROR("[Save] Slot '" + slot + "': cannot replace " + path->string() + ": " + error.message());
            return false;
        }
        return true;
    }

    std::optional<SaveGameFile> SaveSlotStore::Read(const std::string& slot) const
    {
        const std::optional<std::filesystem::path> path = SlotPath(slot, "Read");
        if (!path)
            return std::nullopt;

        const std::optional<std::string> text = ReadText(*path);
        if (!text)
        {
            LOGERROR("[Save] Slot '" + slot + "' does not exist.");
            return std::nullopt;
        }
        SaveGameReadResult<SaveGameFile> result = ReadSaveGame(*text);
        if (!result.Value)
            LOGERROR("[Save] Slot '" + slot + "' cannot be read: " + result.Error + ".");
        return std::move(result.Value);
    }

    std::vector<SaveSlotInfo> SaveSlotStore::List() const
    {
        std::vector<SaveSlotInfo> slots;
        std::error_code           error;
        for (const auto& entry : std::filesystem::directory_iterator(m_Directory, error))
        {
            const std::filesystem::path& path = entry.path();
            const std::string            name = path.stem().string();
            if (!entry.is_regular_file() || path.extension() != SLOT_EXTENSION || !IsValidSlotName(name))
                continue;

            const std::optional<std::string> text = ReadText(path);
            SaveGameReadResult<SaveGameMetadata> header = text ? ReadSaveGameMetadata(*text)
                                                               : SaveGameReadResult<SaveGameMetadata>{ std::nullopt, "cannot be opened" };
            if (!header.Value)
            {
                LOGWARNING("[Save] Slot '" + name + "' is skipped: " + header.Error + ".");
                continue;
            }
            slots.push_back({ name, std::move(*header.Value) });
        }
        std::sort(slots.begin(), slots.end(), [](const SaveSlotInfo& a, const SaveSlotInfo& b) { return a.Name < b.Name; });
        return slots;
    }

    bool SaveSlotStore::Delete(const std::string& slot) const
    {
        const std::optional<std::filesystem::path> path = SlotPath(slot, "Delete");
        if (!path)
            return false;
        std::error_code error;
        return std::filesystem::remove(*path, error);
    }

    bool SaveSlotStore::Exists(const std::string& slot) const
    {
        const std::optional<std::filesystem::path> path = SlotPath(slot, "Exists");
        std::error_code                            error;
        return path && std::filesystem::is_regular_file(*path, error);
    }

    const std::filesystem::path& SaveSlotStore::GetDirectory() const { return m_Directory; }

    std::optional<std::filesystem::path> SaveSlotStore::SlotPath(const std::string& slot, const char* call) const
    {
        if (!IsValidSlotName(slot))
        {
            LOGERROR("[Save] " + std::string(call) + ": '" + slot +
                     "' is not a slot name (1 to 64 of A-Z, a-z, 0-9, '_' and '-').");
            return std::nullopt;
        }
        return m_Directory / (slot + SLOT_EXTENSION);
    }
}
