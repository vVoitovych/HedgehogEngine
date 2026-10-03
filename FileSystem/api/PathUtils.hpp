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

    // Returns the directory containing the running executable.
    // Only implemented on Windows; calls GetModuleFileNameA internally.
    FILE_SYSTEM_API std::filesystem::path GetExecutableDirectory();

    // Returns the engine repository root, computed by walking 3 levels up from
    // the executable directory (Binaries/<Platform>/<Config>/).
    // Asserts that the resulting path exists.
    FILE_SYSTEM_API std::filesystem::path GetEngineRootDirectory();

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
