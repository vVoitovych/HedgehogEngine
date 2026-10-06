#include "FileSystem/api/PathUtils.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <string_view>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace FS
{
    std::filesystem::path GetExecutableDirectory()
    {
#ifdef _WIN32
        char buffer[MAX_PATH];
        const DWORD len = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        assert(len > 0 && len < MAX_PATH && "GetModuleFileNameA failed or path truncated");
        return std::filesystem::path(buffer).parent_path();
#else
        assert(false && "GetExecutableDirectory is not implemented on this platform.");
        return {};
#endif
    }

    namespace
    {
        std::filesystem::path& EngineRootOverride()
        {
            static std::filesystem::path root;
            return root;
        }

        std::filesystem::path& ProjectRootOverride()
        {
            static std::filesystem::path root;
            return root;
        }
    }

    std::optional<std::filesystem::path> FindAncestorHolding(const std::filesystem::path& start,
                                                             const std::filesystem::path& relativeFile)
    {
        std::error_code error;
        for (std::filesystem::path directory = start; !directory.empty(); directory = directory.parent_path())
        {
            if (std::filesystem::is_regular_file(directory / relativeFile, error))
                return directory;
            if (directory == directory.parent_path())
                break;
        }
        return std::nullopt;
    }

    std::filesystem::path FindProjectRoot(const std::filesystem::path& start)
    {
        return FindAncestorHolding(start, PROJECT_FILE_NAME).value_or(start);
    }

    std::filesystem::path FindEngineRoot(const std::filesystem::path& start)
    {
        return FindAncestorHolding(start, ENGINE_ROOT_MARKER).value_or(start);
    }

    std::filesystem::path GetEngineRootDirectory()
    {
        if (!EngineRootOverride().empty())
            return EngineRootOverride();
        return FindEngineRoot(GetExecutableDirectory());
    }

    void SetEngineRootDirectory(const std::filesystem::path& root) { EngineRootOverride() = root; }

    std::filesystem::path GetProjectRootDirectory()
    {
        if (!ProjectRootOverride().empty())
            return ProjectRootOverride();
        if (const auto found = FindAncestorHolding(GetExecutableDirectory(), PROJECT_FILE_NAME))
            return *found;
        return GetEngineRootDirectory();
    }

    void SetProjectRootDirectory(const std::filesystem::path& root) { ProjectRootOverride() = root; }

    std::optional<std::filesystem::path> GetLocalAppDataDirectory()
    {
#ifdef _WIN32
        char*  value  = nullptr;
        size_t length = 0;
        if (_dupenv_s(&value, &length, "LOCALAPPDATA") != 0 || value == nullptr)
        {
            LOGERROR("GetLocalAppDataDirectory: LOCALAPPDATA is not set.");
            return std::nullopt;
        }
        const std::filesystem::path localAppData(value);
        std::free(value);
        return localAppData;
#else
        LOGERROR("GetLocalAppDataDirectory is not implemented on this platform.");
        return std::nullopt;
#endif
    }

    std::filesystem::path MakeEditorUserDirectory(const std::filesystem::path& localAppData)
    {
        return localAppData / "HedgehogEngine" / "Editor";
    }

    std::optional<std::filesystem::path> GetEditorUserDirectory()
    {
        if (const auto localAppData = GetLocalAppDataDirectory())
            return MakeEditorUserDirectory(*localAppData);
        return std::nullopt;
    }

    std::optional<std::filesystem::path>
        MakeSavesDirectory(const std::filesystem::path& localAppData, const std::string& projectName, bool editor)
    {
        constexpr std::string_view FORBIDDEN = "<>:\"/\\|?*";
        if (projectName.empty() || projectName == "." || projectName == ".." ||
            projectName.find_first_of(FORBIDDEN) != std::string::npos ||
            std::any_of(projectName.begin(), projectName.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20; }))
            return std::nullopt;

        std::filesystem::path directory = localAppData / "HedgehogEngine" / projectName / "Saves";
        if (editor)
            directory /= "Editor";
        return directory;
    }

    std::optional<std::filesystem::path> GetSavesDirectory(const std::string& projectName, bool editor)
    {
        if (const auto localAppData = GetLocalAppDataDirectory())
            return MakeSavesDirectory(*localAppData, projectName, editor);
        return std::nullopt;
    }

    bool MountSaves(FileSystemManager& manager, const std::filesystem::path& directory)
    {
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error)
        {
            LOGERROR("MountSaves: cannot create", directory.string(), ":", error.message());
            return false;
        }
        auto fileSystem = std::make_unique<FileSystem>();
        if (!fileSystem->RegisterPath(SAVES_ALIAS, directory))
            return false;
        return manager.Register(std::move(fileSystem));
    }
}
