#include "HedgehogAudio/api/AudioEngine.hpp"

#include "AudioClipCache.hpp"

#include "Logger/api/Logger.hpp"

#include "miniaudio.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace HA
{
    namespace
    {
        constexpr uint32_t FALLBACK_CHANNELS    = 2;
        constexpr uint32_t FALLBACK_SAMPLE_RATE = 48000;

        ma_backend ToMiniaudio(AudioBackend backend)
        {
            switch (backend)
            {
            case AudioBackend::Wasapi:      return ma_backend_wasapi;
            case AudioBackend::DirectSound: return ma_backend_dsound;
            case AudioBackend::WinMM:       return ma_backend_winmm;
            case AudioBackend::CoreAudio:   return ma_backend_coreaudio;
            case AudioBackend::PulseAudio:  return ma_backend_pulseaudio;
            case AudioBackend::Alsa:        return ma_backend_alsa;
            case AudioBackend::Null:        return ma_backend_null;
            }
            return ma_backend_null;
        }
    }

    // One sound and the cursor over its clip's samples it plays from.
    struct SoundSlot
    {
        ma_sound            Sound{};
        ma_audio_buffer_ref Buffer{};
        uint32_t            Generation = 0;
        bool                Active     = false;
    };

    AudioEngine::AudioEngine()
        : m_Clips(std::make_unique<AudioClipCache>())
    {
    }

    AudioEngine::~AudioEngine()
    {
        Shutdown();
    }

    bool AudioEngine::Init(const AudioEngineDesc& desc)
    {
        Shutdown();
        if (desc.NoDevice)
            return InitWithoutDevice(desc);

        ma_engine_config config = ma_engine_config_init();
        config.channels         = desc.Channels;
        config.sampleRate       = desc.SampleRate;

        ma_result result = MA_SUCCESS;
        if (!desc.Backends.empty())
        {
            std::vector<ma_backend> backends;
            backends.reserve(desc.Backends.size());
            for (const AudioBackend backend : desc.Backends)
                backends.push_back(ToMiniaudio(backend));

            m_Context = std::make_unique<ma_context>();
            result    = ma_context_init(backends.data(), static_cast<ma_uint32>(backends.size()), nullptr, m_Context.get());
            if (result == MA_SUCCESS)
                config.pContext = m_Context.get();
            else
                m_Context.reset();
        }

        if (result == MA_SUCCESS)
        {
            m_Engine = std::make_unique<ma_engine>();
            result   = ma_engine_init(&config, m_Engine.get());
            if (result != MA_SUCCESS)
                m_Engine.reset();
        }

        if (result != MA_SUCCESS)
        {
            LOGERROR("[Audio] No output device could be opened (" + std::string(ma_result_description(result)) +
                     "); sound is silent.");
            if (m_Context)
            {
                ma_context_uninit(m_Context.get());
                m_Context.reset();
            }
            return InitWithoutDevice(desc);
        }

        m_HasDevice = true;
        SetMasterVolume(m_MasterVolume);
        return true;
    }

    bool AudioEngine::InitWithoutDevice(const AudioEngineDesc& desc)
    {
        ma_engine_config config = ma_engine_config_init();
        config.noDevice         = MA_TRUE;
        config.channels         = desc.Channels != 0 ? desc.Channels : FALLBACK_CHANNELS;
        config.sampleRate       = desc.SampleRate != 0 ? desc.SampleRate : FALLBACK_SAMPLE_RATE;

        m_Engine                = std::make_unique<ma_engine>();
        const ma_result result  = ma_engine_init(&config, m_Engine.get());
        if (result != MA_SUCCESS)
        {
            LOGERROR("[Audio] The audio engine could not start (" + std::string(ma_result_description(result)) + ").");
            m_Engine.reset();
            return false;
        }
        m_HasDevice = false;
        SetMasterVolume(m_MasterVolume);
        return true;
    }

    void AudioEngine::Shutdown()
    {
        StopAll();
        if (m_Engine)
        {
            ma_engine_uninit(m_Engine.get());
            m_Engine.reset();
        }
        if (m_Context)
        {
            ma_context_uninit(m_Context.get());
            m_Context.reset();
        }
        m_HasDevice = false;
    }

    bool AudioEngine::IsInitialized() const
    {
        return m_Engine != nullptr;
    }

    bool AudioEngine::HasDevice() const
    {
        return m_HasDevice;
    }

    uint32_t AudioEngine::GetChannels() const
    {
        return m_Engine ? ma_engine_get_channels(m_Engine.get()) : 0;
    }

    uint32_t AudioEngine::GetSampleRate() const
    {
        return m_Engine ? ma_engine_get_sample_rate(m_Engine.get()) : 0;
    }

    void AudioEngine::SetMasterVolume(float volume)
    {
        if (!std::isfinite(volume))
            return;
        m_MasterVolume = std::clamp(volume, 0.0f, 1.0f);
        if (m_Engine)
            ma_engine_set_volume(m_Engine.get(), m_MasterVolume);
    }

    float AudioEngine::GetMasterVolume() const
    {
        return m_Engine ? ma_engine_get_volume(m_Engine.get()) : m_MasterVolume;
    }

    AudioClipId AudioEngine::LoadClip(const std::string& virtualPath, const FS::FileSystemManager& files)
    {
        return m_Clips->Load(virtualPath, files);
    }

    float AudioEngine::GetClipLength(AudioClipId clip) const
    {
        const DecodedClip* decoded = m_Clips->Find(clip);
        return decoded ? static_cast<float>(decoded->FrameCount) / static_cast<float>(decoded->SampleRate) : 0.0f;
    }

    SoundHandle AudioEngine::Play(AudioClipId clip, const PlayParams& params)
    {
        const DecodedClip* decoded = m_Clips->Find(clip);
        if (!m_Engine || !decoded)
            return {};

        uint32_t index = 0;
        if (!m_FreeSlots.empty())
        {
            index = m_FreeSlots.back();
            m_FreeSlots.pop_back();
        }
        else
        {
            index = static_cast<uint32_t>(m_Slots.size());
            m_Slots.push_back(std::make_unique<SoundSlot>());
        }
        SoundSlot& slot = *m_Slots[index];

        ma_result result = ma_audio_buffer_ref_init(ma_format_f32, decoded->Channels, decoded->Samples.data(),
                                                    decoded->FrameCount, &slot.Buffer);
        if (result == MA_SUCCESS)
        {
            slot.Buffer.sampleRate = decoded->SampleRate; // resampled to the engine's rate as it plays
            const ma_uint32 flags  = params.Spatial ? 0u : static_cast<ma_uint32>(MA_SOUND_FLAG_NO_SPATIALIZATION);
            result = ma_sound_init_from_data_source(m_Engine.get(), &slot.Buffer, flags, nullptr, &slot.Sound);
            if (result != MA_SUCCESS)
                ma_audio_buffer_ref_uninit(&slot.Buffer);
        }
        if (result != MA_SUCCESS)
        {
            LOGERROR("[Audio] A sound could not start (" + std::string(ma_result_description(result)) + ").");
            m_FreeSlots.push_back(index);
            return {};
        }

        slot.Active = true;
        ma_sound_set_looping(&slot.Sound, params.Loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_min_distance(&slot.Sound, params.MinDistance);
        ma_sound_set_max_distance(&slot.Sound, params.MaxDistance);
        ma_sound_set_rolloff(&slot.Sound, params.Rolloff);
        const SoundHandle handle{ index, slot.Generation };
        SetVolume(handle, params.Volume);
        SetPitch(handle, params.Pitch);
        SetSoundPose(handle, params.Position, params.Velocity);
        ma_sound_start(&slot.Sound);
        return handle;
    }

    SoundSlot* AudioEngine::FindSlot(SoundHandle sound) const
    {
        if (sound.Index >= m_Slots.size())
            return nullptr;
        SoundSlot* slot = m_Slots[sound.Index].get();
        return slot->Active && slot->Generation == sound.Generation ? slot : nullptr;
    }

    void AudioEngine::FreeSlot(uint32_t index)
    {
        SoundSlot& slot = *m_Slots[index];
        ma_sound_uninit(&slot.Sound);
        ma_audio_buffer_ref_uninit(&slot.Buffer);
        slot.Active = false;
        ++slot.Generation;
        m_FreeSlots.push_back(index);
    }

    void AudioEngine::Stop(SoundHandle sound)
    {
        if (FindSlot(sound))
            FreeSlot(sound.Index);
    }

    void AudioEngine::StopAll()
    {
        for (uint32_t index = 0; index < m_Slots.size(); ++index)
        {
            if (m_Slots[index]->Active)
                FreeSlot(index);
        }
    }

    bool AudioEngine::IsPlaying(SoundHandle sound) const
    {
        const SoundSlot* slot = FindSlot(sound);
        return slot && ma_sound_is_playing(&slot->Sound) && !ma_sound_at_end(&slot->Sound);
    }

    bool AudioEngine::Exists(SoundHandle sound) const
    {
        return FindSlot(sound) != nullptr;
    }

    void AudioEngine::SetPaused(SoundHandle sound, bool paused)
    {
        SoundSlot* slot = FindSlot(sound);
        if (!slot)
            return;
        // ma_sound_stop keeps the cursor, so starting again resumes.
        if (paused)
            ma_sound_stop(&slot->Sound);
        else if (!ma_sound_at_end(&slot->Sound))
            ma_sound_start(&slot->Sound);
    }

    void AudioEngine::SetVolume(SoundHandle sound, float volume)
    {
        if (SoundSlot* slot = FindSlot(sound); slot && !std::isnan(volume))
            ma_sound_set_volume(&slot->Sound, std::max(volume, 0.0f));
    }

    void AudioEngine::SetPitch(SoundHandle sound, float pitch)
    {
        if (SoundSlot* slot = FindSlot(sound); slot && pitch > 0.0f && std::isfinite(pitch))
            ma_sound_set_pitch(&slot->Sound, pitch);
    }

    void AudioEngine::SetSoundPose(SoundHandle sound, const HM::Vector3& position, const HM::Vector3& velocity)
    {
        if (SoundSlot* slot = FindSlot(sound))
        {
            ma_sound_set_position(&slot->Sound, position.x(), position.y(), position.z());
            ma_sound_set_velocity(&slot->Sound, velocity.x(), velocity.y(), velocity.z());
        }
    }

    void AudioEngine::SetListenerPose(const HM::Vector3& position, const HM::Vector3& forward, const HM::Vector3& up)
    {
        if (!m_Engine)
            return;
        ma_engine_listener_set_position(m_Engine.get(), 0, position.x(), position.y(), position.z());
        ma_engine_listener_set_direction(m_Engine.get(), 0, forward.x(), forward.y(), forward.z());
        ma_engine_listener_set_world_up(m_Engine.get(), 0, up.x(), up.y(), up.z());
    }

    void AudioEngine::Update()
    {
        for (uint32_t index = 0; index < m_Slots.size(); ++index)
        {
            const SoundSlot& slot = *m_Slots[index];
            if (slot.Active && ma_sound_at_end(&slot.Sound) && !ma_sound_is_looping(&slot.Sound))
                FreeSlot(index);
        }
    }

    size_t AudioEngine::GetActiveSoundCount() const
    {
        return m_Slots.size() - m_FreeSlots.size();
    }

    uint64_t AudioEngine::ReadFrames(float* out, uint64_t frameCount)
    {
        if (!m_Engine || m_HasDevice || !out)
            return 0;
        ma_uint64 framesRead = 0;
        ma_engine_read_pcm_frames(m_Engine.get(), out, frameCount, &framesRead);
        // A graph with nothing playing reads no frames: what it did not write is silence.
        const uint64_t channels = ma_engine_get_channels(m_Engine.get());
        std::fill(out + framesRead * channels, out + frameCount * channels, 0.0f);
        return frameCount;
    }
}
