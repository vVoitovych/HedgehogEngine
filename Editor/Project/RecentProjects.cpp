#include "Project/RecentProjects.hpp"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace Editor
{
    namespace
    {
        constexpr const char* PROJECT_FILE_NAME = "Project.yaml";

        // The normalized path folded to one spelling: forward slashes, lower case.
        std::string ComparisonKey(const std::filesystem::path& path)
        {
            std::string key = NormalizeProjectPath(path).generic_string();
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return key;
        }
    }

    std::filesystem::path NormalizeProjectPath(const std::filesystem::path& path)
    {
        std::error_code       error;
        std::filesystem::path absolute = std::filesystem::absolute(path, error);
        if (error)
            absolute = path;
        absolute = absolute.lexically_normal();
        // "C:/Games/Mine/" normalizes to "C:/Games/Mine/" with an empty last element; a root
        // ("C:/") keeps its separator.
        if (!absolute.has_filename() && absolute.has_relative_path())
            absolute = absolute.parent_path();
        return absolute;
    }

    bool IsSameProject(const std::filesystem::path& a, const std::filesystem::path& b)
    {
        return ComparisonKey(a) == ComparisonKey(b);
    }

    RecentProject* FindRecentProject(std::vector<RecentProject>& projects, const std::filesystem::path& path)
    {
        const auto found = std::find_if(projects.begin(), projects.end(),
                                        [&](const RecentProject& entry) { return IsSameProject(entry.Path, path); });
        return found != projects.end() ? &*found : nullptr;
    }

    const RecentProject* FindRecentProject(const std::vector<RecentProject>& projects, const std::filesystem::path& path)
    {
        const auto found = std::find_if(projects.begin(), projects.end(),
                                        [&](const RecentProject& entry) { return IsSameProject(entry.Path, path); });
        return found != projects.end() ? &*found : nullptr;
    }

    RecentProject& TouchRecentProject(std::vector<RecentProject>& projects, const std::filesystem::path& path)
    {
        RecentProject entry{ NormalizeProjectPath(path), {} };
        if (RecentProject* existing = FindRecentProject(projects, path))
        {
            entry.LastScene = std::move(existing->LastScene);
            projects.erase(projects.begin() + (existing - projects.data()));
        }
        projects.insert(projects.begin(), std::move(entry));
        if (projects.size() > MAX_RECENT_PROJECTS)
            projects.resize(MAX_RECENT_PROJECTS);
        return projects.front();
    }

    size_t RemoveMissingProjects(std::vector<RecentProject>& projects)
    {
        const size_t before = projects.size();
        std::erase_if(projects,
                      [](const RecentProject& entry)
                      {
                          std::error_code error;
                          return !std::filesystem::is_regular_file(entry.Path / PROJECT_FILE_NAME, error);
                      });
        return before - projects.size();
    }
}
