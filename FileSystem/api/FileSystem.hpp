#pragma once

#include "FileSystemApi.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace FS
{
    class FileSystem
    {
    public:
        FILE_SYSTEM_API FileSystem() = default;
        FILE_SYSTEM_API ~FileSystem() = default;

        FileSystem(const FileSystem&)            = delete;
        FileSystem& operator=(const FileSystem&) = delete;
        FileSystem(FileSystem&&)                 = delete;
        FileSystem& operator=(FileSystem&&)      = delete;

        // Maps alias (e.g. "engine://") to an existing physical directory.
        // alias must contain "://" (e.g. "engine://"); bare names like "engine" are rejected.
        // Returns false and logs if: alias lacks "://", alias already registered here,
        // physicalPath is not an existing directory, or canonicalization fails.
        FILE_SYSTEM_API bool RegisterPath(const std::string& alias,
                                          const std::filesystem::path& physicalPath);

        // Resolves "alias://rel/path" and returns raw file bytes.
        // Returns nullopt and logs on unrecognised alias or file open failure.
        FILE_SYSTEM_API std::optional<std::vector<std::byte>>
            ReadFile(const std::string& virtualPath) const;

        // Convenience: reads and returns file content as a string.
        // Returns nullopt under the same conditions as ReadFile.
        // The result may contain embedded null bytes; do not pass .c_str() to C-string APIs.
        FILE_SYSTEM_API std::optional<std::string>
            ReadTextFile(const std::string& virtualPath) const;

        // Writes binary data to virtualPath. Creates parent directories as needed.
        // Returns false and logs on unrecognised alias or write failure.
        FILE_SYSTEM_API bool WriteFile(const std::string& virtualPath,
                                       const std::vector<std::byte>& data) const;

        // Writes a string as text to virtualPath. Creates parent directories as needed.
        // Returns false and logs on unrecognised alias or write failure.
        FILE_SYSTEM_API bool WriteTextFile(const std::string& virtualPath,
                                           const std::string& text) const;

        // Returns true if the file at virtualPath exists on disk.
        // Returns false for an unrecognised alias or a missing file.
        FILE_SYSTEM_API bool Exists(const std::string& virtualPath) const;

        // Returns the physical path for virtualPath, or nullopt if alias unknown.
        // Use only as an escape hatch for third-party loaders that require an OS path.
        FILE_SYSTEM_API std::optional<std::filesystem::path>
            ResolvePhysical(const std::string& virtualPath) const;

        // True if this instance has alias registered as a mount point.
        FILE_SYSTEM_API bool OwnsAlias(const std::string& alias) const;

        // True if the alias portion of virtualPath is registered here,
        // regardless of whether the physical file exists.
        FILE_SYSTEM_API bool OwnsPath(const std::string& virtualPath) const;

        FILE_SYSTEM_API const std::unordered_map<std::string, std::filesystem::path>&
            GetMountPoints() const;

    private:
        // Splits "engine://textures/foo.png" into {"engine://", "textures/foo.png"}.
        // Returns nullopt when "://" is absent.
        static std::optional<std::pair<std::string, std::string>>
            ParseVirtualPath(const std::string& virtualPath);

        // Returns the physical path for virtualPath, or nullopt if alias unknown.
        std::optional<std::filesystem::path>
            Resolve(const std::string& virtualPath) const;

    private:
        std::unordered_map<std::string, std::filesystem::path> m_MountPoints;
    };
}
