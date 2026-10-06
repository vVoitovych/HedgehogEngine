#pragma once

#include "FileSystemApi.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace FS
{
    class FileSystemManager;

    // Where save games live: a per-user folder outside the project, mounted as saves://.
    inline constexpr const char* SAVES_ALIAS = "saves://";

    // The project's own files (Project.yaml, its settings), mounted at the project root.
    inline constexpr const char* PROJECT_ALIAS = "project://";

    // The game's assets, mounted at the project's Assets/ folder.
    inline constexpr const char* ASSETS_ALIAS = "assets://";

    // The engine's own runtime content (default meshes and textures, the base script, the editor
    // font), at Content/ under the engine root: engine://Content/...
    inline constexpr const char* ENGINE_CONTENT_PREFIX = "engine://Content/";

    // A component's asset path as a virtual path: one naming a mount ("engine://Content/a.obj")
    // is kept, any other ("Models\a.obj", "Models/a.obj") goes under assets://.
    FILE_SYSTEM_API std::string ToAssetVirtualPath(const std::string& path);

    // The file that marks a project's root: the repository in a dev tree, the folder of a packaged game.
    inline constexpr const char* PROJECT_FILE_NAME = "Project.yaml";

    // The file that marks the engine's root in a dev tree (the repository). A packaged game needs
    // none: its executable sits at the package's root.
    inline constexpr const char* ENGINE_ROOT_MARKER = "Engine.yaml";

    // Returns the directory containing the running executable.
    // Only implemented on Windows; calls GetModuleFileNameA internally.
    FILE_SYSTEM_API std::filesystem::path GetExecutableDirectory();

    // The nearest of start and its ancestors holding the regular file relativeFile, or nullopt.
    FILE_SYSTEM_API std::optional<std::filesystem::path> FindAncestorHolding(const std::filesystem::path& start,
                                                                             const std::filesystem::path& relativeFile);

    // The nearest of start and its ancestors that holds PROJECT_FILE_NAME, or start itself when
    // none does.
    FILE_SYSTEM_API std::filesystem::path FindProjectRoot(const std::filesystem::path& start);

    // The nearest of start and its ancestors that holds ENGINE_ROOT_MARKER, or start itself when
    // none does.
    FILE_SYSTEM_API std::filesystem::path FindEngineRoot(const std::filesystem::path& start);

    // The engine:// root: the directory SetEngineRootDirectory named, else FindEngineRoot of the
    // executable's directory, which finds the repository from Binaries/<Platform>/<Config>/ and
    // falls back to the executable's directory (a packaged game's own folder).
    FILE_SYSTEM_API std::filesystem::path GetEngineRootDirectory();

    // Overrides GetEngineRootDirectory for the rest of the process; call it before the engine is
    // built. An empty path clears the override.
    FILE_SYSTEM_API void SetEngineRootDirectory(const std::filesystem::path& root);

    // The project:// root: the directory SetProjectRootDirectory named, else the nearest folder
    // above the executable holding PROJECT_FILE_NAME, else the engine root. In a dev tree and a
    // package alike both roots are, for now, the same folder.
    FILE_SYSTEM_API std::filesystem::path GetProjectRootDirectory();

    // Overrides GetProjectRootDirectory for the rest of the process (an opened project); call it
    // before the engine is built. An empty path clears the override.
    FILE_SYSTEM_API void SetProjectRootDirectory(const std::filesystem::path& root);

    // %LOCALAPPDATA%; nullopt, logged, when it is not set (or off Windows).
    FILE_SYSTEM_API std::optional<std::filesystem::path> GetLocalAppDataDirectory();

    // <localAppData>/HedgehogEngine/Editor: the editor's per-user state (layout, recent projects).
    FILE_SYSTEM_API std::filesystem::path MakeEditorUserDirectory(const std::filesystem::path& localAppData);

    // MakeEditorUserDirectory under %LOCALAPPDATA%; nullopt, logged, when it is not set.
    FILE_SYSTEM_API std::optional<std::filesystem::path> GetEditorUserDirectory();

    // <localAppData>/HedgehogEngine/<projectName>/Saves, and its Editor subfolder for the editor's
    // play sessions, so they never overwrite a shipped game's saves. nullopt for a projectName
    // that is not one plain folder name (empty, ".", "..", or holding a slash, colon or other
    // character a folder name cannot take).
    FILE_SYSTEM_API std::optional<std::filesystem::path>
        MakeSavesDirectory(const std::filesystem::path& localAppData, const std::string& projectName, bool editor);

    // MakeSavesDirectory under %LOCALAPPDATA%; nullopt, logged, when it is not set.
    FILE_SYSTEM_API std::optional<std::filesystem::path> GetSavesDirectory(const std::string& projectName, bool editor);

    // Creates directory if needed and mounts it as saves:// in manager. False, logged, when it
    // cannot be created or saves:// is already mounted.
    FILE_SYSTEM_API bool MountSaves(FileSystemManager& manager, const std::filesystem::path& directory);
}
