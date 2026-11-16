#pragma once

#include "HedgehogEngine/api/HedgehogEngineApi.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffect.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace HedgehogEngine
{
    // The loaded particle effects, one per file however its path is spelt (FS::MakeAssetKey), each
    // read again once its file changes. Every file tried keeps its index for the container's
    // lifetime, failures included, so an index never moves.
    class ParticleEffectContainer
    {
    public:
        // How often ReloadChanged looks at the files' write times.
        static constexpr std::chrono::seconds POLL_INTERVAL{ 1 };

        HEDGEHOG_ENGINE_API explicit ParticleEffectContainer(const FS::FileSystemManager& fileSystem);

        // The index of path (under assets:// unless it names a mount), reading it on first use.
        // std::nullopt for an empty path or a file that has never read and parsed; that is logged
        // once (`[Particles] <path>: ...`) and not read again until its write time moves on.
        HEDGEHOG_ENGINE_API std::optional<size_t> FindOrLoad(const std::string& path);

        HEDGEHOG_ENGINE_API size_t GetCount() const;
        // The file's key (FS::MakeAssetKey); FS::ToAssetVirtualPath of it names the file.
        HEDGEHOG_ENGINE_API const std::string& GetPath(size_t index) const;
        // The effect, the last version that read; nullptr when none ever has.
        HEDGEHOG_ENGINE_API const ParticleEffect* GetEffect(size_t index) const;
        // Bumped each time a new version of the effect is read, so users know to restart it.
        HEDGEHOG_ENGINE_API uint32_t GetVersion(size_t index) const;

        // At most once per POLL_INTERVAL (now is a parameter so tests need not wait), reads again
        // every file whose write time has moved on: a good one replaces its effect, bumps its
        // version and logs `[Particles] Reloaded <path>.`; a broken one is logged and its last good
        // effect kept.
        HEDGEHOG_ENGINE_API void ReloadChanged(std::chrono::steady_clock::time_point now);

        // An edit not saved yet, shown in place of path's file (the editor's live preview): GetEffect
        // gives it until ClearOverride, and each call bumps the version. SetOverride reads the file
        // first when it is new to the container, and works whether or not it loads.
        HEDGEHOG_ENGINE_API void SetOverride(const std::string& path, ParticleEffect effect);
        HEDGEHOG_ENGINE_API void ClearOverride(const std::string& path);

    private:
        struct Entry
        {
            std::string                     Key;
            std::optional<ParticleEffect>   Effect;
            std::optional<ParticleEffect>   Override;
            std::filesystem::file_time_type WriteTime{};
            uint32_t                        Version = 0;
        };

        // Reads the entry's file; false (logged) when it does not read or parse.
        bool Load(Entry& entry);
        // The entry of path, read on first use; nullptr for an empty path.
        Entry* FindOrAdd(const std::string& path);

        const FS::FileSystemManager&          m_FileSystem;
        std::vector<Entry>                    m_Entries;
        std::chrono::steady_clock::time_point m_LastPoll{};
    };
}
