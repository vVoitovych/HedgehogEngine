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
        auto physPath = resolve(virtualPath);
        if (!physPath)
        {
            LOGERROR("[FileSystem] Cannot resolve virtual path '", virtualPath, "'.");
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

    bool FileSystem::OwnsAlias(const std::string& alias) const
    {
        return m_MountPoints.count(alias) > 0;
    }

    bool FileSystem::OwnsPath(const std::string& virtualPath) const
    {
        auto parsed = parseVirtualPath(virtualPath);
        if (!parsed)
            return false;
        return m_MountPoints.count(parsed->first) > 0;
    }

    const std::unordered_map<std::string, std::filesystem::path>& FileSystem::GetMountPoints() const
    {
        return m_MountPoints;
    }

    std::optional<std::pair<std::string, std::string>> FileSystem::parseVirtualPath(const std::string& virtualPath)
    {
        const auto pos = virtualPath.find("://");
        if (pos == std::string::npos)
            return std::nullopt;

        return std::make_pair(
            virtualPath.substr(0, pos + 3),
            virtualPath.substr(pos + 3)
        );
    }

    std::optional<std::filesystem::path> FileSystem::resolve(const std::string& virtualPath) const
    {
        auto parsed = parseVirtualPath(virtualPath);
        if (!parsed)
            return std::nullopt;

        const auto it = m_MountPoints.find(parsed->first);
        if (it == m_MountPoints.end())
            return std::nullopt;

        return it->second / parsed->second;
    }
}
