#include "EcsSerialization/api/SaveGame/SaveGameFile.hpp"

#include <stdexcept>
#include <vector>

namespace EcsSerialization
{
    namespace
    {
        // The line that starts the sections document.
        constexpr const char* DOCUMENT_SEPARATOR = "\n---\n";

        template<typename T>
        T ReadField(const YAML::Node& metadata, const char* name, const char* expected)
        {
            const YAML::Node field = metadata[name];
            T                value{};
            if (!field || !YAML::convert<T>::decode(field, value))
                throw std::runtime_error(std::string("Metadata.") + name + " is missing or is not " + expected);
            return value;
        }

        SaveGameMetadata ReadMetadata(const YAML::Node& header)
        {
            const YAML::Node metadata = header["Metadata"];
            if (!metadata || !metadata.IsMap())
                throw std::runtime_error("the save has no Metadata map");

            SaveGameMetadata result;
            result.SaveVersion     = ReadField<int>(metadata, "SaveVersion", "an integer");
            result.GameDataVersion = ReadField<int>(metadata, "GameDataVersion", "an integer");
            result.ScenePath       = ReadField<std::string>(metadata, "Scene", "a string");
            result.Timestamp       = ReadField<std::string>(metadata, "Timestamp", "a string");
            result.PlayTime        = ReadField<double>(metadata, "PlayTime", "a number");
            return result;
        }
    }

    std::string WriteSaveGame(const SaveGameFile& save)
    {
        YAML::Emitter header;
        header << YAML::BeginMap << YAML::Key << "Metadata" << YAML::Value << YAML::BeginMap;
        header << YAML::Key << "SaveVersion" << YAML::Value << save.Metadata.SaveVersion;
        header << YAML::Key << "GameDataVersion" << YAML::Value << save.Metadata.GameDataVersion;
        header << YAML::Key << "Scene" << YAML::Value << save.Metadata.ScenePath;
        header << YAML::Key << "Timestamp" << YAML::Value << save.Metadata.Timestamp;
        header << YAML::Key << "PlayTime" << YAML::Value << save.Metadata.PlayTime;
        header << YAML::EndMap << YAML::EndMap;

        YAML::Emitter sections;
        sections << YAML::BeginMap << YAML::Key << "Sections" << YAML::Value << YAML::BeginMap;
        for (const auto& [name, node] : save.Sections)
            sections << YAML::Key << name << YAML::Value << node;
        sections << YAML::EndMap << YAML::EndMap;

        return std::string(header.c_str()) + DOCUMENT_SEPARATOR + sections.c_str() + "\n";
    }

    SaveGameReadResult<SaveGameMetadata> ReadSaveGameMetadata(const std::string& text)
    {
        try
        {
            const size_t separator = text.find(DOCUMENT_SEPARATOR);
            return { ReadMetadata(YAML::Load(text.substr(0, separator))), {} };
        }
        catch (const std::exception& e)
        {
            return { std::nullopt, e.what() };
        }
    }

    SaveGameReadResult<SaveGameFile> ReadSaveGame(const std::string& text)
    {
        try
        {
            const std::vector<YAML::Node> documents = YAML::LoadAll(text);
            if (documents.size() != 2)
                return { std::nullopt, "a save holds 2 YAML documents, not " + std::to_string(documents.size()) };

            SaveGameFile save;
            save.Metadata = ReadMetadata(documents[0]);
            if (save.Metadata.SaveVersion < 1 || save.Metadata.SaveVersion > SAVE_FORMAT_VERSION)
                return { std::nullopt, "the save is format version " + std::to_string(save.Metadata.SaveVersion) +
                                           ", but this build reads versions 1 to " + std::to_string(SAVE_FORMAT_VERSION) };

            const YAML::Node sections = documents[1]["Sections"];
            if (!sections || !sections.IsMap())
                return { std::nullopt, "the save has no Sections map" };
            for (const auto& section : sections)
                save.Sections[section.first.as<std::string>()] = section.second;
            return { std::move(save), {} };
        }
        catch (const std::exception& e)
        {
            return { std::nullopt, e.what() };
        }
    }
}
