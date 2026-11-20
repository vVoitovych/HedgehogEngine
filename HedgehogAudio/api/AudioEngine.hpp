#pragma once

#include "HedgehogAudio/api/HedgehogAudioApi.hpp"
#include "HedgehogAudio/api/SoundHandle.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

// miniaudio's types, kept opaque: only HedgehogAudio's sources include miniaudio.h.
struct ma_engine;
struct ma_context;
struct ma_sound;

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

    // The mixer buses every sound plays through, each with a volume of its own (SetBusVolume). The
    // order and names are those of HedgehogSettings' PLAYER_AUDIO_BUSES; Master is the engine's
    // SetMasterVolume.
    enum class AudioBus
    {
        Music,
        Sfx,
        Voice,
        Ui,
    };

    inline constexpr size_t AUDIO_BUS_COUNT = 4;

    inline constexpr std::array<std::string_view, AUDIO_BUS_COUNT> AUDIO_BUS_NAMES = { "Music", "Sfx", "Voice", "Ui" };

    // Empty for a value out of range.
    [[nodiscard]] inline std::string_view GetBusName(AudioBus bus)
    {
        const size_t index = static_cast<size_t>(bus);
        return index < AUDIO_BUS_COUNT ? AUDIO_BUS_NAMES[index] : std::string_view();
    }

    // Case-sensitive; nullopt for a name that is no bus.
    [[nodiscard]] inline std::optional<AudioBus> FindBus(std::string_view name)
    {
        for (size_t i = 0; i < AUDIO_BUS_COUNT; ++i)
        {
            if (AUDIO_BUS_NAMES[i] == name)
            {
                return static_cast<AudioBus>(i);
            }
        }
        return std::nullopt;
    }

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
        AudioBus    Bus         = AudioBus::Sfx; // the mixer bus it plays through
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

        // A bus's linear gain, applied to every sound on it: what is heard is the sound's volume times
        // its bus's times the master volume. Clamped to [0, 1]; a non-finite value or a bus out of range
        // is ignored. Kept across Init and Shutdown as the master volume is.
        HEDGEHOG_AUDIO_API void SetBusVolume(AudioBus bus, float volume);
        [[nodiscard]] HEDGEHOG_AUDIO_API float GetBusVolume(AudioBus bus) const; // 0 for a bus out of range

        // Reads and decodes a clip (WAV, FLAC or MP3) under a virtual path through files, once: a
        // path loaded before returns the same id without touching the file. A file that cannot be
        // read or decoded logs one [Audio] error naming the path, is remembered as failed and gives
        // an invalid id. Clips do not depend on the engine running and outlive Shutdown.
        HEDGEHOG_AUDIO_API AudioClipId LoadClip(const std::string& virtualPath, const FS::FileSystemManager& files);
        // In seconds; 0 for an invalid id.
        [[nodiscard]] HEDGEHOG_AUDIO_API float GetClipLength(AudioClipId clip) const;
        // Clips decoded and kept by LoadClip (failed paths not counted).
        [[nodiscard]] HEDGEHOG_AUDIO_API size_t GetCachedClipCount() const;

        // Starts a sound of clip. An invalid handle when the clip is invalid or the engine is not
        // running.
        HEDGEHOG_AUDIO_API SoundHandle Play(AudioClipId clip, const PlayParams& params);
        // Starts a sound that streams the file at virtualPath (resolved to its physical file through
        // files) from disk: miniaudio's resource manager pages through it on its own thread, so nothing
        // is decoded ahead and no clip is cached; two streams of one file read it twice. For long music.
        // A path that does not resolve to a file, or a file that cannot be opened, logs one [Audio]
        // error per path and gives an invalid handle. Every other call takes its handle as Play's.
        HEDGEHOG_AUDIO_API SoundHandle PlayStream(const std::string& virtualPath, const FS::FileSystemManager& files,
                                                  const PlayParams& params);
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
        // so tests and offline rendering advance its sounds; 0 otherwise. Rendering offline, it first waits
        // (at most STREAM_WAIT) for each stream to have the frames paged in, so a stream reads as its file.
        HEDGEHOG_AUDIO_API uint64_t ReadFrames(float* out, uint64_t frameCount);
        static constexpr std::chrono::milliseconds STREAM_WAIT{ 2000 };

    private:
        bool                     InitWithoutDevice(const AudioEngineDesc& desc);
        void                     InitBuses();
        [[nodiscard]] SoundSlot* FindSlot(SoundHandle sound) const;
        // A free slot's index, reused or new.
        uint32_t                 AcquireSlot();
        // Applies params to the slot's initialized sound and starts it.
        SoundHandle              StartSlot(uint32_t index, const PlayParams& params);
        [[nodiscard]] ma_sound*  GetBusGroup(AudioBus bus) const;
        void                     WaitForStreams(uint64_t frameCount) const;
        void                     FreeSlot(uint32_t index);

        std::unique_ptr<ma_context>                            m_Context; // only when Backends picks them
        std::unique_ptr<ma_engine>                             m_Engine;
        bool                                                   m_HasDevice    = false;
        float                                                  m_MasterVolume = 1.0f;
        std::array<std::unique_ptr<ma_sound>, AUDIO_BUS_COUNT> m_Buses; // one ma_sound_group each
        std::array<float, AUDIO_BUS_COUNT>                     m_BusVolumes   = { 1.0f, 1.0f, 1.0f, 1.0f };
        std::unique_ptr<AudioClipCache>                        m_Clips;
        std::vector<std::unique_ptr<SoundSlot>>                m_Slots; // stable addresses: miniaudio holds them
        std::vector<uint32_t>                                  m_FreeSlots;
        std::unordered_set<std::string>                        m_StreamFailures; // logged once each
    };
}
