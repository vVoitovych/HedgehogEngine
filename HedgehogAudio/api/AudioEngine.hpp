#pragma once

#include "HedgehogAudio/api/HedgehogAudioApi.hpp"
#include "HedgehogAudio/api/SoundHandle.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// miniaudio's types, kept opaque: only HedgehogAudio's sources include miniaudio.h.
struct ma_engine;
struct ma_context;

namespace FS
{
    class FileSystemManager;
}

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

    // How a sound plays. A spatial sound is placed in the world and heard from the listener (see
    // SetListenerPose): panned by direction and attenuated by distance, inversely from MinDistance,
    // by Rolloff, up to MaxDistance. A sound that is not spatial plays as is.
    struct PlayParams
    {
        float       Volume      = 1.0f; // linear gain, at least 0
        float       Pitch       = 1.0f; // playback rate, above 0
        bool        Loop        = false;
        bool        Spatial     = false;
        HM::Vector3 Position    = HM::Vector3(0.0f, 0.0f, 0.0f);
        HM::Vector3 Velocity    = HM::Vector3(0.0f, 0.0f, 0.0f); // for the Doppler effect
        float       MinDistance = 1.0f;
        float       MaxDistance = 1000.0f;
        float       Rolloff     = 1.0f;
    };

    class AudioClipCache;
    struct SoundSlot;

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

        // Reads and decodes a clip (WAV, FLAC or MP3) under a virtual path through files, once: a
        // path loaded before returns the same id without touching the file. A file that cannot be
        // read or decoded logs one [Audio] error naming the path, is remembered as failed and gives
        // an invalid id. Clips do not depend on the engine running and outlive Shutdown.
        HEDGEHOG_AUDIO_API AudioClipId LoadClip(const std::string& virtualPath, const FS::FileSystemManager& files);
        // In seconds; 0 for an invalid id.
        [[nodiscard]] HEDGEHOG_AUDIO_API float GetClipLength(AudioClipId clip) const;

        // Starts a sound of clip. An invalid handle when the clip is invalid or the engine is not
        // running.
        HEDGEHOG_AUDIO_API SoundHandle Play(AudioClipId clip, const PlayParams& params);
        // Stops a sound and frees its slot, so its handle goes stale. A stale handle does nothing.
        HEDGEHOG_AUDIO_API void Stop(SoundHandle sound);
        HEDGEHOG_AUDIO_API void StopAll();
        // True from Play until the sound is paused, stopped or, when it does not loop, reaches its end.
        [[nodiscard]] HEDGEHOG_AUDIO_API bool IsPlaying(SoundHandle sound) const;
        // True while the sound holds its slot: playing, paused, or finished and not yet reclaimed by
        // Update. False for a stale handle.
        [[nodiscard]] HEDGEHOG_AUDIO_API bool Exists(SoundHandle sound) const;
        // Pausing holds a sound where it is, keeping its slot; resuming carries on from there.
        HEDGEHOG_AUDIO_API void SetPaused(SoundHandle sound, bool paused);
        HEDGEHOG_AUDIO_API void SetVolume(SoundHandle sound, float volume); // clamped to at least 0
        HEDGEHOG_AUDIO_API void SetPitch(SoundHandle sound, float pitch);   // ignored unless above 0
        // A spatial sound's place and velocity in the world.
        HEDGEHOG_AUDIO_API void SetSoundPose(SoundHandle sound, const HM::Vector3& position, const HM::Vector3& velocity);
        // Where the sounds are heard from: forward and up are directions (the engine's cameras look
        // down -Z with +Y up, as the listener does before any call). Reset by Init.
        HEDGEHOG_AUDIO_API void SetListenerPose(const HM::Vector3& position, const HM::Vector3& forward, const HM::Vector3& up);

        // Frees the slots of sounds that finished on their own; call once a frame. Their handles go
        // stale.
        HEDGEHOG_AUDIO_API void Update();
        // Sounds holding a slot: playing, or finished and not yet reclaimed by Update.
        [[nodiscard]] HEDGEHOG_AUDIO_API size_t GetActiveSoundCount() const;

        // Mixes frameCount frames of every sound into out (interleaved floats, GetChannels() per
        // frame, silence when nothing plays) and returns frameCount. Only an engine without a device is read this way,
        // so tests and offline rendering advance its sounds; 0 otherwise.
        HEDGEHOG_AUDIO_API uint64_t ReadFrames(float* out, uint64_t frameCount);

    private:
        bool                     InitWithoutDevice(const AudioEngineDesc& desc);
        [[nodiscard]] SoundSlot* FindSlot(SoundHandle sound) const;
        void                     FreeSlot(uint32_t index);

        std::unique_ptr<ma_context>             m_Context; // only when Backends picks them
        std::unique_ptr<ma_engine>              m_Engine;
        bool                                    m_HasDevice    = false;
        float                                   m_MasterVolume = 1.0f;
        std::unique_ptr<AudioClipCache>         m_Clips;
        std::vector<std::unique_ptr<SoundSlot>> m_Slots;     // stable addresses: miniaudio holds them
        std::vector<uint32_t>                   m_FreeSlots;
    };
}
