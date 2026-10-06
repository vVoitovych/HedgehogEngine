#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/FileSystem.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>

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
            if (IsAliasTaken(alias))
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

    bool FileSystemManager::WriteFile(const std::string& virtualPath,
                                       const std::vector<std::byte>& data) const
    {
        const FileSystem* owner = FindOwner(virtualPath);
        if (!owner)
            return false;
        return owner->WriteFile(virtualPath, data);
    }

    bool FileSystemManager::WriteTextFile(const std::string& virtualPath,
                                           const std::string& text) const
    {
        const FileSystem* owner = FindOwner(virtualPath);
        if (!owner)
            return false;
        return owner->WriteTextFile(virtualPath, text);
    }

    bool FileSystemManager::Exists(const std::string& virtualPath) const
    {
        const FileSystem* owner = FindOwner(virtualPath);
        if (!owner)
            return false;
        return owner->Exists(virtualPath);
    }

    std::optional<std::vector<DirectoryEntry>> FileSystemManager::ListDirectory(const std::string& virtualPath) const
    {
        const FileSystem* owner = FindOwner(virtualPath);
        if (!owner)
            return std::nullopt;
        return owner->ListDirectory(virtualPath);
    }

    std::optional<std::filesystem::path> FileSystemManager::ResolvePhysical(
        const std::string& virtualPath) const
    {
        const FileSystem* owner = FindOwner(virtualPath);
        if (!owner)
            return std::nullopt;
        return owner->ResolvePhysical(virtualPath);
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

    std::optional<std::string> FileSystemManager::ToVirtualPath(
        const std::filesystem::path& absolutePath) const
    {
        std::filesystem::path canonical;
        try
        {
            canonical = std::filesystem::canonical(absolutePath);
        }
        catch (const std::filesystem::filesystem_error&)
        {
            // weakly_canonical is best-effort for paths about to be created; paths containing
            // unresolved '..' may escape the mount root and fail the prefix match below.
            canonical = std::filesystem::weakly_canonical(absolutePath);
        }

        // Mounts may nest (assets:// inside engine://), so the one with the longest root that
        // holds the path names it; on a tie the alias that sorts first, so the answer never
        // depends on map order.
        const std::string*           bestAlias = nullptr;
        const std::filesystem::path* bestRoot  = nullptr;
        size_t                       bestDepth = 0;
        for (const auto& fs : m_FileSystems)
        {
            for (const auto& [alias, physicalRoot] : fs->GetMountPoints())
            {
                auto [mountIt, inputIt] =
                    std::mismatch(physicalRoot.begin(), physicalRoot.end(), canonical.begin(), canonical.end());
                if (mountIt != physicalRoot.end())
                    continue;
                const size_t depth = static_cast<size_t>(std::distance(physicalRoot.begin(), physicalRoot.end()));
                if (!bestAlias || depth > bestDepth || (depth == bestDepth && alias < *bestAlias))
                {
                    bestAlias = &alias;
                    bestRoot  = &physicalRoot;
                    bestDepth = depth;
                }
            }
        }
        if (bestAlias)
        {
            const std::filesystem::path rel = canonical.lexically_relative(*bestRoot);
            return *bestAlias + rel.generic_string();
        }

        LOGWARNING("[FileSystemManager] ToVirtualPath: '", absolutePath.string(),
                   "' is not under any registered mount point.");
        return std::nullopt;
    }

    bool FileSystemManager::IsAliasTaken(const std::string& alias) const
    {
        for (const auto& fs : m_FileSystems)
        {
            if (fs->OwnsAlias(alias))
                return true;
        }
        return false;
    }
}
