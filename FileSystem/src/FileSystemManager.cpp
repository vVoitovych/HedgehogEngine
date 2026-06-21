#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/FileSystem.hpp"

#include "Logger/api/Logger.hpp"

namespace FS
{
    bool FileSystemManager::Register(std::unique_ptr<FileSystem> fileSystem)
    {
        const auto& mountPoints = fileSystem->GetMountPoints();
        if (mountPoints.empty())
        {
            LOGWARNING("[FileSystemManager] Attempted to register a FileSystem with no mount points.");
            return false;
        }

        for (const auto& [alias, path] : mountPoints)
        {
            if (isAliasTaken(alias))
            {
                LOGERROR("[FileSystemManager] Alias '", alias,
                    "' is already registered in another FileSystem.");
                return false;
            }
        }
        m_FileSystems.push_back(std::move(fileSystem));
        return true;
    }

    const FileSystem* FileSystemManager::FindOwner(const std::string& virtualPath) const
    {
        const FileSystem* found = nullptr;
        size_t count = 0;

        for (const auto& fs : m_FileSystems)
        {
            if (fs->OwnsPath(virtualPath))
            {
                found = fs.get();
                ++count;
            }
        }

        if (count == 1)
            return found;

        if (count == 0)
            LOGERROR("[FileSystemManager] No FileSystem owns virtual path '", virtualPath, "'.");
        else
            // count > 1 is unreachable under current invariants: Register() rejects duplicate
            // aliases, so no two FileSystems can own the same virtual path prefix.
            LOGERROR("[FileSystemManager] Ambiguous virtual path '", virtualPath,
                "': ", count, " FileSystems claim it.");

        return nullptr;
    }

    std::optional<std::vector<std::byte>> FileSystemManager::ReadFile(const std::string& virtualPath) const
    {
        const FileSystem* owner = FindOwner(virtualPath);
        if (!owner)
            return std::nullopt;
        return owner->ReadFile(virtualPath);
    }

    std::optional<std::string> FileSystemManager::ReadTextFile(const std::string& virtualPath) const
    {
        const FileSystem* owner = FindOwner(virtualPath);
        if (!owner)
            return std::nullopt;
        return owner->ReadTextFile(virtualPath);
    }

    std::unique_ptr<FileSystem> FileSystemManager::Unregister(const std::string& alias)
    {
        for (auto it = m_FileSystems.begin(); it != m_FileSystems.end(); ++it)
        {
            if ((*it)->OwnsAlias(alias))
            {
                auto fs = std::move(*it);
                m_FileSystems.erase(it);
                return fs;
            }
        }
        return nullptr;
    }

    const std::vector<std::unique_ptr<FileSystem>>& FileSystemManager::GetFileSystems() const
    {
        return m_FileSystems;
    }

    bool FileSystemManager::isAliasTaken(const std::string& alias) const
    {
        for (const auto& fs : m_FileSystems)
        {
            if (fs->OwnsAlias(alias))
                return true;
        }
        return false;
    }
}
