#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace Editor
{
    // The folder File > New Project... creates: <parent>/<name>.
    [[nodiscard]] std::filesystem::path MakeNewProjectFolder(std::string_view name, const std::filesystem::path& parent);

    // Why a project name cannot be created under parent (a name ProjectSettings::IsValidName
    // refuses, a parent that is not an existing folder, or a <parent>/<name> that exists and is not
    // an empty folder), or empty when it can. ImGui-free, so EditorTest checks it.
    [[nodiscard]] std::string CheckNewProject(std::string_view name, const std::filesystem::path& parent);
}
