#include "AudioClipCache.hpp"

#include "FileSystem/api/FileSystemManager.hpp"
#include "Logger/api/Logger.hpp"

#include "miniaudio.h"

#include <optional>

namespace HA
{
    namespace
    {
        constexpr ma_uint64 DECODE_CHUNK_FRAMES = 4096;

        // The whole file as float frames at its own format; logs and gives nullopt on failure.
        std::optional<DecodedClip> Decode(const std::string& virtualPath, const std::vector<std::byte>& bytes)
        {
            const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
            ma_decoder              decoder;
            const ma_result         result = ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder);
            if (result != MA_SUCCESS)
            {
                LOGERROR("[Audio]", virtualPath + ": the file cannot be decoded (" +
                                        std::string(ma_result_description(result)) + ").");
                return std::nullopt;
            }

            DecodedClip clip;
            clip.Channels   = decoder.outputChannels;
            clip.SampleRate = decoder.outputSampleRate;
            ma_uint64 length = 0;
            if (ma_decoder_get_length_in_pcm_frames(&decoder, &length) == MA_SUCCESS)
                clip.Samples.reserve(static_cast<size_t>(length * clip.Channels));

            // Read in chunks: a format may not know its length up front.
            std::vector<float> chunk(static_cast<size_t>(DECODE_CHUNK_FRAMES * clip.Channels));
            while (true)
            {
                ma_uint64 framesRead = 0;
                ma_decoder_read_pcm_frames(&decoder, chunk.data(), DECODE_CHUNK_FRAMES, &framesRead);
                if (framesRead == 0)
                    break;
                clip.Samples.insert(clip.Samples.end(), chunk.begin(),
                                    chunk.begin() + static_cast<std::ptrdiff_t>(framesRead * clip.Channels));
                clip.FrameCount += framesRead;
            }
            ma_decoder_uninit(&decoder);

            if (clip.FrameCount == 0)
            {
                LOGERROR("[Audio]", virtualPath + ": the file has no audio.");
                return std::nullopt;
            }
            return clip;
        }
    }

    AudioClipId AudioClipCache::Load(const std::string& virtualPath, const FS::FileSystemManager& files)
    {
        if (const auto found = m_ByPath.find(virtualPath); found != m_ByPath.end())
            return found->second;

        AudioClipId                                 id;
        const std::optional<std::vector<std::byte>> bytes = files.ReadFile(virtualPath);
        if (!bytes)
        {
            LOGERROR("[Audio]", virtualPath + ": the file cannot be read.");
        }
        else if (std::optional<DecodedClip> clip = Decode(virtualPath, *bytes))
        {
            id.Index = static_cast<uint32_t>(m_Clips.size());
            m_Clips.push_back(std::move(*clip));
        }
        m_ByPath.emplace(virtualPath, id);
        return id;
    }

    const DecodedClip* AudioClipCache::Find(AudioClipId id) const
    {
        return id.Index < m_Clips.size() ? &m_Clips[id.Index] : nullptr;
    }
}
