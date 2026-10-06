#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// The editor's recent projects, kept in its per-user editor_settings.yaml: plain data and free
// functions, no ImGui, so EditorTest compiles them directly.
namespace Editor
{
    // The most projects the list remembers.
    inline constexpr size_t MAX_RECENT_PROJECTS = 10;

    struct RecentProject
    {
        std::filesystem::path Path;      // the project folder, absolute and normalized
        std::string           LastScene; // the scene it last had open (virtual path), or empty
    };

    // A project folder as the list stores it: absolute, lexically normal, without a trailing
    // separator.
    [[nodiscard]] std::filesystem::path NormalizeProjectPath(const std::filesystem::path& path);

    // Whether two spellings name one folder: normalized, then compared ignoring case and the kind
    // of slash, as Windows names folders.
    [[nodiscard]] bool IsSameProject(const std::filesystem::path& a, const std::filesystem::path& b);

    // The entry of path, or nullptr.
    [[nodiscard]] RecentProject*       FindRecentProject(std::vector<RecentProject>& projects, const std::filesystem::path& path);
    [[nodiscard]] const RecentProject* FindRecentProject(const std::vector<RecentProject>& projects,
                                                         const std::filesystem::path& path);

    // Moves path's entry to the front, keeping its last scene, or adds one there; drops the
    // oldest past MAX_RECENT_PROJECTS. Returns the entry, valid until the list next changes.
    RecentProject& TouchRecentProject(std::vector<RecentProject>& projects, const std::filesystem::path& path);

    // Drops every entry whose folder no longer holds Project.yaml; returns how many it dropped.
    size_t RemoveMissingProjects(std::vector<RecentProject>& projects);
}
