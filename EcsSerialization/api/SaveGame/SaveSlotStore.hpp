#pragma once

#include "EcsSerialization/api/EcsSerializationApi.hpp"
#include "EcsSerialization/api/SaveGame/SaveGameFile.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace EcsSerialization
{
    // One slot of a listing: its name and its header, read without parsing the world.
    struct SaveSlotInfo
    {
        std::string      Name;
        SaveGameMetadata Metadata;
    };

    // Save slots in one directory (the mounted saves:// folder, or a test's temp folder), each the
    // file <slot>.save. A slot name is 1 to 64 of A-Z, a-z, 0-9, '_' and '-', so no name reaches
    // outside the directory; every call with another name logs an error and fails.
    class SaveSlotStore
    {
    public:
        static constexpr const char* SLOT_EXTENSION = ".save";
        static constexpr const char* TEMP_EXTENSION = ".tmp";
        static constexpr size_t      MAX_SLOT_NAME  = 64;

        ECS_SERIALIZATION_API explicit SaveSlotStore(std::filesystem::path directory);

        [[nodiscard]] ECS_SERIALIZATION_API static bool IsValidSlotName(std::string_view slot);

        // Writes save to <slot>.tmp, then renames it over <slot>.save, so a write that fails or is
        // cut short leaves the slot's previous save readable. Creates the directory if needed.
        ECS_SERIALIZATION_API bool Write(const std::string& slot, const SaveGameFile& save) const;

        // The whole save; nullopt, logged with the slot and the reason, when it is missing or does
        // not read (ReadSaveGame).
        [[nodiscard]] ECS_SERIALIZATION_API std::optional<SaveGameFile> Read(const std::string& slot) const;

        // Every slot whose header reads, by name; a file whose header does not read is skipped with
        // a warning, and leftover .tmp files are ignored. Empty when the directory does not exist.
        [[nodiscard]] ECS_SERIALIZATION_API std::vector<SaveSlotInfo> List() const;

        // Removes the slot's file; false when there was none.
        ECS_SERIALIZATION_API bool Delete(const std::string& slot) const;

        [[nodiscard]] ECS_SERIALIZATION_API bool Exists(const std::string& slot) const;

        [[nodiscard]] ECS_SERIALIZATION_API const std::filesystem::path& GetDirectory() const;

    private:
        // The slot's file, or nullopt (logged, naming call) for an invalid name.
        std::optional<std::filesystem::path> SlotPath(const std::string& slot, const char* call) const;

    private:
        std::filesystem::path m_Directory;
    };
}
