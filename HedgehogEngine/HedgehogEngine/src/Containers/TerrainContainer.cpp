#include "HedgehogEngine/api/Containers/TerrainContainer.hpp"

#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    TerrainContainer::TerrainContainer(const FS::FileSystemManager& fileSystem)
        : m_FileSystem(fileSystem)
    {
    }

    std::optional<size_t> TerrainContainer::FindOrLoad(const std::string& path)
    {
        if (path.empty())
            return std::nullopt;

        const std::string key   = FS::MakeAssetKey(path);
        const auto        found = std::find_if(m_Entries.begin(), m_Entries.end(), [&](const Entry& entry) { return entry.Path == key; });
        if (found != m_Entries.end())
        {
            if (!found->Data)
                return std::nullopt;
            const size_t entryIndex = static_cast<size_t>(found - m_Entries.begin());
            return static_cast<size_t>(std::find(m_Loaded.begin(), m_Loaded.end(), entryIndex) - m_Loaded.begin());
        }

        Entry             entry;
        const std::string virtualPath = FS::ToAssetVirtualPath(key);
        entry.Path                    = key;
        if (const std::optional<std::string> text = m_FileSystem.ReadTextFile(virtualPath))
        {
            TerrainReadResult result = ParseTerrain(*text);
            if (result.Data)
                entry.Data = std::move(result.Data);
            else
                LOGERROR("[Terrain]", virtualPath + ":", result.Error + ".");
        }
        else
        {
            LOGERROR("[Terrain]", virtualPath, "cannot be read.");
        }

        if (entry.Data)
        {
            entry.Dirty = static_cast<uint8_t>(TerrainDirty::All);
            m_Loaded.push_back(m_Entries.size());
        }
        const bool loaded = entry.Data.has_value();
        m_Entries.push_back(std::move(entry));
        if (!loaded)
            return std::nullopt;
        return m_Loaded.size() - 1;
    }

    size_t TerrainContainer::GetTerrainCount() const
    {
        return m_Loaded.size();
    }

    const std::string& TerrainContainer::GetPath(size_t index) const
    {
        return m_Entries[m_Loaded[index]].Path;
    }

    const TerrainData& TerrainContainer::GetTerrain(size_t index) const
    {
        return *m_Entries[m_Loaded[index]].Data;
    }

    TerrainData& TerrainContainer::GetTerrain(size_t index)
    {
        return *m_Entries[m_Loaded[index]].Data;
    }

    void TerrainContainer::MarkDirty(size_t index, TerrainDirty parts)
    {
        m_Entries[m_Loaded[index]].Dirty |= static_cast<uint8_t>(parts);
    }

    TerrainDirty TerrainContainer::GetDirty(size_t index) const
    {
        return static_cast<TerrainDirty>(m_Entries[m_Loaded[index]].Dirty);
    }

    void TerrainContainer::ClearDirty(size_t index, TerrainDirty parts)
    {
        m_Entries[m_Loaded[index]].Dirty &= static_cast<uint8_t>(~static_cast<uint8_t>(parts));
    }

    bool TerrainContainer::Save(size_t index) const
    {
        const Entry&      entry       = m_Entries[m_Loaded[index]];
        const std::string virtualPath = FS::ToAssetVirtualPath(entry.Path);
        if (!m_FileSystem.WriteTextFile(virtualPath, WriteTerrain(*entry.Data)))
        {
            LOGERROR("[Terrain]", virtualPath, "cannot be written.");
            return false;
        }
        LOGINFO("[Terrain] Saved", virtualPath + ".");
        return true;
    }
}
