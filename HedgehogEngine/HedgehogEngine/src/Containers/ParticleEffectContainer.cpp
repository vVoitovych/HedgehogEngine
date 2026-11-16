#include "HedgehogEngine/api/Containers/ParticleEffectContainer.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffectFile.hpp"

#include "FileSystem/api/PathUtils.hpp"
#include "Logger/api/Logger.hpp"

#include <algorithm>

namespace HedgehogEngine
{
    namespace
    {
        std::filesystem::file_time_type WriteTimeOf(const FS::FileSystemManager& files, const std::string& virtualPath)
        {
            const std::optional<std::filesystem::path> physical = files.ResolvePhysical(virtualPath);
            std::error_code                            error;
            return physical ? std::filesystem::last_write_time(*physical, error) : std::filesystem::file_time_type{};
        }
    }

    ParticleEffectContainer::ParticleEffectContainer(const FS::FileSystemManager& fileSystem)
        : m_FileSystem(fileSystem)
    {
    }

    bool ParticleEffectContainer::Load(Entry& entry)
    {
        const std::string virtualPath = FS::ToAssetVirtualPath(entry.Key);
        entry.WriteTime               = WriteTimeOf(m_FileSystem, virtualPath);
        const std::optional<std::string> text = m_FileSystem.ReadTextFile(virtualPath);
        if (!text)
        {
            LOGERROR("[Particles] " + virtualPath + " cannot be read.");
            return false;
        }
        ParticleEffectReadResult result = ParseParticleEffect(*text);
        if (!result.Effect)
        {
            LOGERROR("[Particles] " + virtualPath + ": " + result.Error + ".");
            return false;
        }
        entry.Effect = std::move(result.Effect);
        ++entry.Version;
        return true;
    }

    ParticleEffectContainer::Entry* ParticleEffectContainer::FindOrAdd(const std::string& path)
    {
        if (path.empty())
            return nullptr;
        const std::string key   = FS::MakeAssetKey(path);
        const auto        found = std::find_if(m_Entries.begin(), m_Entries.end(), [&](const Entry& entry) { return entry.Key == key; });
        if (found != m_Entries.end())
            return &*found;
        m_Entries.push_back(Entry{ key });
        Load(m_Entries.back());
        return &m_Entries.back();
    }

    std::optional<size_t> ParticleEffectContainer::FindOrLoad(const std::string& path)
    {
        const Entry* entry = FindOrAdd(path);
        if (!entry || (!entry->Effect && !entry->Override))
            return std::nullopt;
        return static_cast<size_t>(entry - m_Entries.data());
    }

    void ParticleEffectContainer::SetOverride(const std::string& path, ParticleEffect effect)
    {
        if (Entry* entry = FindOrAdd(path))
        {
            entry->Override = std::move(effect);
            ++entry->Version;
        }
    }

    void ParticleEffectContainer::ClearOverride(const std::string& path)
    {
        if (Entry* entry = FindOrAdd(path); entry && entry->Override)
        {
            entry->Override.reset();
            ++entry->Version;
        }
    }

    size_t ParticleEffectContainer::GetCount() const
    {
        return m_Entries.size();
    }

    const std::string& ParticleEffectContainer::GetPath(size_t index) const
    {
        return m_Entries[index].Key;
    }

    const ParticleEffect* ParticleEffectContainer::GetEffect(size_t index) const
    {
        const Entry& entry = m_Entries[index];
        if (entry.Override)
            return &*entry.Override;
        return entry.Effect ? &*entry.Effect : nullptr;
    }

    uint32_t ParticleEffectContainer::GetVersion(size_t index) const
    {
        return m_Entries[index].Version;
    }

    void ParticleEffectContainer::ReloadChanged(std::chrono::steady_clock::time_point now)
    {
        if (m_Entries.empty() || now - m_LastPoll < POLL_INTERVAL)
            return;
        m_LastPoll = now;
        for (Entry& entry : m_Entries)
        {
            const std::string virtualPath = FS::ToAssetVirtualPath(entry.Key);
            if (WriteTimeOf(m_FileSystem, virtualPath) == entry.WriteTime)
                continue;
            // Load leaves a broken version's last good effect alone and takes its write time, so it
            // is not read again until the next save.
            if (Load(entry))
                LOGINFO("[Particles] Reloaded " + virtualPath + ".");
        }
    }
}
