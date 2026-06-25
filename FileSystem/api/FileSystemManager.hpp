#pragma once

#include "FileSystemApi.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace FS
{
    class FileSystem;

    class FileSystemManager
    {
    public:
        FILE_SYSTEM_API FileSystemManager() = default;
        FILE_SYSTEM_API ~FileSystemManager() = default;

        FileSystemManager(const FileSystemManager&)            = delete;
        FileSystemManager& operator=(const FileSystemManager&) = delete;
        FileSystemManager(FileSystemManager&&)                 = delete;
        FileSystemManager& operator=(FileSystemManager&&)      = delete;

        // Takes ownership of fileSystem.
        // Returns false and logs on alias collision; the instance is destroyed.
        FILE_SYSTEM_API bool Register(std::unique_ptr<FileSystem> fileSystem);

        // Logs and returns nullptr when 0 or more than 1 owners are found.
        FILE_SYSTEM_API const FileSystem* FindOwner(const std::string& virtualPath) const;

        // Resolves and reads via whichever FileSystem owns virtualPath.
        FILE_SYSTEM_API std::optional<std::vector<std::byte>>
            ReadFile(const std::string& virtualPath) const;

        FILE_SYSTEM_API std::optional<std::string>
            ReadTextFile(const std::string& virtualPath) const;

        // Writes binary data through whichever FileSystem owns virtualPath.
        FILE_SYSTEM_API bool WriteFile(const std::string& virtualPath,
                                       const std::vector<std::byte>& data) const;

        // Writes text through whichever FileSystem owns virtualPath.
        FILE_SYSTEM_API bool WriteTextFile(const std::string& virtualPath,
                                           const std::string& text) const;

        // Returns true if the file at virtualPath exists on disk.
        FILE_SYSTEM_API bool Exists(const std::string& virtualPath) const;

        // Returns the physical OS path for virtualPath, or nullopt if the alias is unknown.
        FILE_SYSTEM_API std::optional<std::filesystem::path>
            ResolvePhysical(const std::string& virtualPath) const;

        // Removes and returns the FileSystem that owns alias. Returns nullptr if not found.
        // Removes the entire FileSystem object including all its other aliases.
        FILE_SYSTEM_API std::unique_ptr<FileSystem> Unregister(const std::string& alias);

        // Converts an absolute OS path to a virtual path (e.g. "assets://foo/bar.obj") by
        // matching against the physical root of each registered mount point.
        // Returns nullopt and logs a warning when absolutePath is not under any known mount.
        FILE_SYSTEM_API std::optional<std::string>
            ToVirtualPath(const std::filesystem::path& absolutePath) const;

        FILE_SYSTEM_API const std::vector<std::unique_ptr<FileSystem>>& GetFileSystems() const;

    private:
        bool IsAliasTaken(const std::string& alias) const;

    private:
        std::vector<std::unique_ptr<FileSystem>> m_FileSystems;
    };
}
