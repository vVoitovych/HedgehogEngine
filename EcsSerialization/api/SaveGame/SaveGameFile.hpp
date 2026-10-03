#pragma once

#include "EcsSerialization/api/EcsSerializationApi.hpp"

#include "yaml-cpp/yaml.h"

#include <map>
#include <optional>
#include <string>

// A save game as text: a metadata header document, then a document of named sections, so a slot
// listing reads the header alone and never parses the world. Plain data and free functions.
namespace EcsSerialization
{
    // The save file format this build writes and reads. A newer save is refused.
    inline constexpr int SAVE_FORMAT_VERSION = 1;

    // The section names the engine writes; a game may add its own.
    inline constexpr const char* SAVE_SECTION_WORLD   = "World";
    inline constexpr const char* SAVE_SECTION_SCRIPTS = "Scripts";

    struct SaveGameMetadata
    {
        int         SaveVersion     = SAVE_FORMAT_VERSION; // the file format
        int         GameDataVersion = 1;                   // the game's own data version, bumped by the game
        std::string ScenePath;                             // the scene the save was made in
        std::string Timestamp;                             // when it was saved, as the writer formats it (ISO 8601 UTC)
        double      PlayTime        = 0.0;                 // seconds played

        bool operator==(const SaveGameMetadata&) const = default;
    };

    struct SaveGameFile
    {
        SaveGameMetadata                  Metadata;
        std::map<std::string, YAML::Node> Sections; // by name; written in name order
    };

    template<typename T>
    struct SaveGameReadResult
    {
        std::optional<T> Value;
        std::string      Error; // why Value is empty
    };

    // The file's text: "Metadata" in the first document, "Sections" in the second.
    [[nodiscard]] ECS_SERIALIZATION_API std::string WriteSaveGame(const SaveGameFile& save);

    // The header alone: the text up to the second document is all that is parsed. A save of a
    // newer format still reads, so a listing can show it; ReadSaveGame refuses it.
    [[nodiscard]] ECS_SERIALIZATION_API SaveGameReadResult<SaveGameMetadata> ReadSaveGameMetadata(const std::string& text);

    // The whole save. Fails, naming the problem, for malformed YAML, a missing or mistyped
    // metadata field, a SaveVersion newer than SAVE_FORMAT_VERSION or below 1, or no Sections map.
    [[nodiscard]] ECS_SERIALIZATION_API SaveGameReadResult<SaveGameFile> ReadSaveGame(const std::string& text);
}
