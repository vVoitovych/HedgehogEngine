#pragma once

#include "HedgehogSettingsApi.hpp"

#include <filesystem>
#include <string>

namespace HedgehogSettings
{
    // The template a new project starts from, relative to the engine root: a complete project
    // (Project.yaml, engine_settings.yaml, input actions, a scene, a material and empty asset
    // folders), so what a new project holds is data, not code. Templates are never packaged.
    inline constexpr const char* EMPTY_TEMPLATE_DIRECTORY = "Templates/Empty";

    // The files a template keeps only so that git keeps its empty folders; never copied.
    inline constexpr const char* FOLDER_PLACEHOLDER_NAME = ".gitkeep";

    // Creates the project name at targetDir from the template at templateDir: copies every file
    // and folder (placeholders left out), then names it through ProjectSettings::SetName and
    // saves its Project.yaml. targetDir must not exist or be an empty folder. Returns an empty
    // string on success, else why it failed (a name ProjectSettings::IsValidName refuses, a
    // template without Project.yaml, a target that is not empty, a copy or save that fails), with
    // everything it created removed.
    [[nodiscard]] HEDGEHOG_SETTINGS_API std::string CreateProject(const std::filesystem::path& templateDir,
                                                                  const std::filesystem::path& targetDir,
                                                                  const std::string&           name);
}
