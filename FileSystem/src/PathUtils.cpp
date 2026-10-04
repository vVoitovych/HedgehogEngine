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
    }

    std::filesystem::path FindProjectRoot(const std::filesystem::path& start)
    {
        std::error_code error;
        for (std::filesystem::path directory = start; !directory.empty(); directory = directory.parent_path())
        {
            if (std::filesystem::is_regular_file(directory / PROJECT_FILE_NAME, error))
                return directory;
            if (directory == directory.parent_path())
                break;
        }
        return start;
    }

    std::filesystem::path GetEngineRootDirectory()
    {
        if (!EngineRootOverride().empty())
            return EngineRootOverride();
        return FindProjectRoot(GetExecutableDirectory());
    }

    void SetEngineRootDirectory(const std::filesystem::path& root) { EngineRootOverride() = root; }

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
#ifdef _WIN32
        char*  value  = nullptr;
        size_t length = 0;
        if (_dupenv_s(&value, &length, "LOCALAPPDATA") != 0 || value == nullptr)
        {
            LOGERROR("GetSavesDirectory: LOCALAPPDATA is not set.");
            return std::nullopt;
        }
        const std::filesystem::path localAppData(value);
        std::free(value);
        return MakeSavesDirectory(localAppData, projectName, editor);
#else
        LOGERROR("GetSavesDirectory is not implemented on this platform.");
        return std::nullopt;
#endif
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
