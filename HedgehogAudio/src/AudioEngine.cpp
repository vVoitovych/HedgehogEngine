#include "HedgehogAudio/api/AudioEngine.hpp"

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

    AudioEngine::AudioEngine() = default;

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
}
