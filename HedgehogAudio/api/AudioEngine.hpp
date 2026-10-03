#pragma once

#include "HedgehogAudio/api/HedgehogAudioApi.hpp"

#include <cstdint>
#include <memory>
#include <vector>

// miniaudio's types, kept opaque: only HedgehogAudio's sources include miniaudio.h.
struct ma_engine;
struct ma_context;

namespace HA
{
    // The platform audio backends miniaudio can open a device through.
    enum class AudioBackend
    {
        Wasapi,
        DirectSound,
        WinMM,
        CoreAudio,
        PulseAudio,
        Alsa,
        Null, // a device that consumes audio without playing it
    };

    struct AudioEngineDesc
    {
        // No output device: the engine mixes only when read, so tests and headless runs need no
        // sound card.
        bool NoDevice = false;
        // The backends to try, in order; empty for miniaudio's default order for the platform.
        std::vector<AudioBackend> Backends;
        // The format asked of the device (0 takes the device's own), and the format of an engine
        // without one (0 falls back to 2 channels at 48 kHz).
        uint32_t Channels   = 2;
        uint32_t SampleRate = 48000;
    };

    // Owns miniaudio's engine, the mixer every sound will play through. An output device that
    // cannot be opened is logged once and the engine runs without one: silent, but valid.
    class AudioEngine
    {
    public:
        HEDGEHOG_AUDIO_API AudioEngine();
        HEDGEHOG_AUDIO_API ~AudioEngine();

        AudioEngine(const AudioEngine&)            = delete;
        AudioEngine& operator=(const AudioEngine&) = delete;

        // Starts the engine, shutting down a running one first, and applies the master volume.
        // Returns IsInitialized(): false only when not even an engine without a device can start.
        HEDGEHOG_AUDIO_API bool Init(const AudioEngineDesc& desc);
        // Stops the engine and closes its device; does nothing when not initialized.
        HEDGEHOG_AUDIO_API void Shutdown();

        [[nodiscard]] HEDGEHOG_AUDIO_API bool IsInitialized() const;
        // Whether the engine plays to an output device (false without one, or after a fallback).
        [[nodiscard]] HEDGEHOG_AUDIO_API bool HasDevice() const;
        [[nodiscard]] HEDGEHOG_AUDIO_API uint32_t GetChannels() const;   // 0 when not initialized
        [[nodiscard]] HEDGEHOG_AUDIO_API uint32_t GetSampleRate() const; // 0 when not initialized

        // Linear gain, clamped to [0, 1]; a non-finite value is ignored. It is kept across Init and
        // Shutdown, and GetMasterVolume reads the engine's own while it runs.
        HEDGEHOG_AUDIO_API void SetMasterVolume(float volume);
        [[nodiscard]] HEDGEHOG_AUDIO_API float GetMasterVolume() const;

    private:
        bool InitWithoutDevice(const AudioEngineDesc& desc);

        std::unique_ptr<ma_context> m_Context; // only when Backends picks them
        std::unique_ptr<ma_engine>  m_Engine;
        bool                        m_HasDevice    = false;
        float                       m_MasterVolume = 1.0f;
    };
}
