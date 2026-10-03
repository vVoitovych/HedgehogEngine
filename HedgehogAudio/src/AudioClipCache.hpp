#pragma once

#include "HedgehogAudio/api/SoundHandle.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace FS
{
    class FileSystemManager;
}

namespace HA
{
    // A clip decoded to interleaved 32-bit float frames at its own channel count and rate.
    struct DecodedClip
    {
        std::vector<float> Samples;
        uint32_t           Channels   = 0;
        uint32_t           SampleRate = 0;
        uint64_t           FrameCount = 0;
    };

    // Clips by virtual path, each decoded once. A path that fails is remembered as failed, so it is
    // logged once and never read again.
    class AudioClipCache
    {
    public:
        AudioClipId Load(const std::string& virtualPath, const FS::FileSystemManager& files);

        // nullptr for an invalid id.
        [[nodiscard]] const DecodedClip* Find(AudioClipId id) const;

    private:
        std::vector<DecodedClip>                       m_Clips;
        std::unordered_map<std::string, AudioClipId>   m_ByPath; // failed paths map to an invalid id
    };
}
