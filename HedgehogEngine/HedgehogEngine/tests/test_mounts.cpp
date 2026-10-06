#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogEngine/HedgehogSettings/api/ProjectSettings.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <filesystem>
#include <optional>

namespace
{
    // Points the project root at a folder for one test, then puts the default back.
    struct ProjectRootOverride
    {
        explicit ProjectRootOverride(const std::filesystem::path& root) { FS::SetProjectRootDirectory(root); }
        ~ProjectRootOverride() { FS::SetProjectRootDirectory({}); }
        ProjectRootOverride(const ProjectRootOverride&)            = delete;
        ProjectRootOverride& operator=(const ProjectRootOverride&) = delete;
    };

    bool IsUnder(const std::optional<std::filesystem::path>& path, const std::filesystem::path& root)
    {
        if (!path)
            return false;
        const std::filesystem::path relative = std::filesystem::weakly_canonical(*path).lexically_relative(
            std::filesystem::weakly_canonical(root));
        return !relative.empty() && *relative.begin() != "..";
    }
}

TEST_CASE("Engine mounts - by default the project is the engine's Projects/FeatureTest")
{
    HedgehogEngine::EngineContext context;
    const FS::FileSystemManager& files = context.GetFileSystem();
    CHECK(files.Exists(HedgehogSettings::ProjectSettings::PATH));
    CHECK(files.Exists(HedgehogSettings::Settings::PATH));
    CHECK(files.Exists("engine://Engine.yaml"));
    CHECK_FALSE(files.Exists("engine://Project.yaml"));

    const std::filesystem::path sample = FS::GetEngineRootDirectory() / FS::DEFAULT_PROJECT_DIRECTORY;
    CHECK(IsUnder(files.ResolvePhysical(HedgehogSettings::ProjectSettings::PATH), sample));
    CHECK(IsUnder(files.ResolvePhysical("assets://Scenes/Default.yaml"), sample));
    CHECK(files.Exists("assets://Scenes/Default.yaml"));
    // A file of the project is named by the project's mounts, never engine://Projects/...
    CHECK(files.ToVirtualPath(sample / "Assets" / "Scenes" / "Default.yaml") == "assets://Scenes/Default.yaml");
    CHECK(files.ToVirtualPath(sample / "Project.yaml") == "project://Project.yaml");
}

TEST_CASE("Engine mounts - a project elsewhere: project:// and assets:// there, engine:// and shaders:// the engine's")
{
    TempDir project;
    project.WriteFile("Project.yaml", "name: Elsewhere\n");
    project.WriteFile("engine_settings.yaml", "lua_debugger: { enabled: false, port: 4711 }\n");
    project.WriteFile("Assets/Notes/readme.txt", "hello\n");
    const ProjectRootOverride override(project.Path());

    HedgehogEngine::EngineContext context;
    const FS::FileSystemManager& files = context.GetFileSystem();

    CHECK(IsUnder(files.ResolvePhysical(HedgehogSettings::ProjectSettings::PATH), project.Path()));
    CHECK(IsUnder(files.ResolvePhysical(HedgehogSettings::Settings::PATH), project.Path()));
    CHECK(IsUnder(files.ResolvePhysical("assets://Notes/readme.txt"), project.Path()));
    CHECK(files.Exists("assets://Notes/readme.txt"));

    const std::filesystem::path engineRoot = FS::GetEngineRootDirectory();
    CHECK(IsUnder(files.ResolvePhysical("engine://Engine.yaml"), engineRoot));
    CHECK_FALSE(IsUnder(files.ResolvePhysical("engine://Engine.yaml"), project.Path()));
    CHECK(files.Exists("engine://Engine.yaml"));
    CHECK(IsUnder(files.ResolvePhysical("shaders://Common"), engineRoot));

    // The project's files are named by the project's mounts, not the engine's.
    CHECK(files.ToVirtualPath(project.Path() / "Assets" / "Notes" / "readme.txt") == "assets://Notes/readme.txt");
    CHECK(files.ToVirtualPath(project.Path() / "Project.yaml") == "project://Project.yaml");
}
