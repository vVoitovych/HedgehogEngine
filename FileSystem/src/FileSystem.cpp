#include "FileSystem/api/FileSystem.hpp"

#include "Logger/api/Logger.hpp"

#include <fstream>

namespace FS
{
    bool FileSystem::RegisterPath(const std::string& alias, const std::filesystem::path& physicalPath)
    {
        if (alias.find("://") == std::string::npos)
        {
            LOGWARNING("[FileSystem] Invalid alias '", alias, "': missing '://' separator.");
            return false;
        }
        if (m_MountPoints.count(alias) > 0)
        {
            LOGWARNING("[FileSystem] Alias '", alias, "' is already registered.");
            return false;
        }
        if (!std::filesystem::is_directory(physicalPath))
        {
            LOGWARNING("[FileSystem] Path '", physicalPath.string(), "' is not an existing directory.");
            return false;
        }
        try
        {
            m_MountPoints[alias] = std::filesystem::canonical(physicalPath);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            LOGERROR("[FileSystem] Failed to canonicalize '", physicalPath.string(), "': ", e.what());
            return false;
        }
        return true;
    }

    std::optional<std::vector<std::byte>> FileSystem::ReadFile(const std::string& virtualPath) const
    {
        auto physPath = Resolve(virtualPath);
        if (!physPath)
        {
            LOGERROR("[FileSystem] Cannot Resolve virtual path '", virtualPath, "'.");
            return std::nullopt;
        }

        if (!std::filesystem::exists(*physPath))
        {
            LOGWARNING("[FileSystem] File not found: ", physPath->string());
            return std::nullopt;
        }

        std::ifstream file(*physPath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            LOGERROR("[FileSystem] Failed to open '", physPath->string(), "'.");
            return std::nullopt;
        }

        const auto size = file.tellg();
        if (size == std::streampos(-1))
        {
            LOGERROR("[FileSystem] Failed to determine size of '", physPath->string(), "'.");
            return std::nullopt;
        }

        if (!file.seekg(0))
        {
            LOGERROR("[FileSystem] Failed to seek to beginning of '", physPath->string(), "'.");
            return std::nullopt;
        }

        std::vector<std::byte> buffer(static_cast<size_t>(size));
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        if (file.gcount() != static_cast<std::streamsize>(size))
        {
            LOGERROR("[FileSystem] Partial read of '", physPath->string(), "': expected ",
                static_cast<std::streamsize>(size), " bytes, got ", file.gcount(), ".");
            return std::nullopt;
        }
        return buffer;
    }

    std::optional<std::string> FileSystem::ReadTextFile(const std::string& virtualPath) const
    {
        auto bytes = ReadFile(virtualPath);
        if (!bytes)
            return std::nullopt;

        return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    }

    bool FileSystem::WriteFile(const std::string& virtualPath,
                                const std::vector<std::byte>& data) const
    {
        auto physPath = Resolve(virtualPath);
        if (!physPath)
        {
            LOGERROR("[FileSystem] Cannot resolve virtual path '", virtualPath, "' for writing.");
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(physPath->parent_path(), ec);
        if (ec)
        {
            LOGERROR("[FileSystem] Failed to create directories for '", physPath->string(),
                "': ", ec.message());
            return false;
        }

        std::ofstream file(*physPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open())
        {
            LOGERROR("[FileSystem] Failed to open '", physPath->string(), "' for writing.");
            return false;
        }

        file.write(reinterpret_cast<const char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));
        return file.good();
    }

    bool FileSystem::WriteTextFile(const std::string& virtualPath, const std::string& text) const
    {
        const std::vector<std::byte> bytes(
            reinterpret_cast<const std::byte*>(text.data()),
            reinterpret_cast<const std::byte*>(text.data()) + text.size());
        return WriteFile(virtualPath, bytes);
    }

    bool FileSystem::Exists(const std::string& virtualPath) const
    {
        auto physPath = Resolve(virtualPath);
        if (!physPath)
            return false;
        return std::filesystem::exists(*physPath);
    }

    std::optional<std::filesystem::path> FileSystem::ResolvePhysical(
        const std::string& virtualPath) const
    {
        return Resolve(virtualPath);
    }

    bool FileSystem::OwnsAlias(const std::string& alias) const
    {
        return m_MountPoints.count(alias) > 0;
    }

    bool FileSystem::OwnsPath(const std::string& virtualPath) const
    {
        auto parsed = ParseVirtualPath(virtualPath);
        if (!parsed)
            return false;
        return m_MountPoints.count(parsed->first) > 0;
    }

    const std::unordered_map<std::string, std::filesystem::path>& FileSystem::GetMountPoints() const
    {
        return m_MountPoints;
    }

    std::optional<std::pair<std::string, std::string>> FileSystem::ParseVirtualPath(const std::string& virtualPath)
    {
        const auto pos = virtualPath.find("://");
        if (pos == std::string::npos)
            return std::nullopt;

        return std::make_pair(
            virtualPath.substr(0, pos + 3),
            virtualPath.substr(pos + 3)
        );
    }

    std::optional<std::filesystem::path> FileSystem::Resolve(const std::string& virtualPath) const
    {
        auto parsed = ParseVirtualPath(virtualPath);
        if (!parsed)
            return std::nullopt;

        const auto it = m_MountPoints.find(parsed->first);
        if (it == m_MountPoints.end())
            return std::nullopt;

        return it->second / parsed->second;
    }
}
