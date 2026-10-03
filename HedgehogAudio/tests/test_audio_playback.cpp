#include "doctest/doctest/doctest.h"

#include "HedgehogAudio/api/AudioEngine.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"
#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

namespace
{
    constexpr uint32_t RATE           = 48000;
    constexpr uint32_t CLIP_FRAMES    = 4800; // 0.1 s
    constexpr uint64_t CHUNK_FRAMES   = 480;

    void Append(std::string& out, uint32_t value, int bytes)
    {
        for (int i = 0; i < bytes; ++i)
            out.push_back(static_cast<char>((value >> (8 * i)) & 0xFF));
    }

    // A mono 16-bit WAV of a 440 Hz sine at half amplitude.
    std::string MakeWav(uint32_t frames)
    {
        std::string data;
        for (uint32_t i = 0; i < frames; ++i)
        {
            const double  t      = static_cast<double>(i) / RATE;
            const int16_t sample = static_cast<int16_t>(16384.0 * std::sin(2.0 * std::numbers::pi * 440.0 * t));
            Append(data, static_cast<uint16_t>(sample), 2);
        }
        std::string wav = "RIFF";
        Append(wav, static_cast<uint32_t>(36 + data.size()), 4);
        wav += "WAVEfmt ";
        Append(wav, 16, 4);       // fmt chunk size
        Append(wav, 1, 2);        // PCM
        Append(wav, 1, 2);        // mono
        Append(wav, RATE, 4);
        Append(wav, RATE * 2, 4); // bytes per second
        Append(wav, 2, 2);        // bytes per frame
        Append(wav, 16, 2);       // bits per sample
        wav += "data";
        Append(wav, static_cast<uint32_t>(data.size()), 4);
        return wav + data;
    }

    // A started engine without a device over a temporary assets:// holding the test clips.
    struct AudioWorld
    {
        TempDir               Dir;
        FS::FileSystemManager Files;
        HA::AudioEngine       Engine;

        AudioWorld()
        {
            Dir.WriteFile("Audio/beep.wav", MakeWav(CLIP_FRAMES));
            Dir.WriteFile("Audio/broken.wav", "this is not audio");
            auto fileSystem = std::make_unique<FS::FileSystem>();
            fileSystem->RegisterPath("assets://", Dir.Path());
            REQUIRE(Files.Register(std::move(fileSystem)));

            HA::AudioEngineDesc desc;
            desc.NoDevice = true;
            REQUIRE(Engine.Init(desc));
        }

        HA::AudioClipId Beep() { return Engine.LoadClip("assets://Audio/beep.wav", Files); }

        // Mixes frames and sums each channel's magnitude.
        void Mix(uint64_t frames, double* left = nullptr, double* right = nullptr)
        {
            std::vector<float> buffer(CHUNK_FRAMES * 2);
            for (uint64_t done = 0; done < frames; done += CHUNK_FRAMES)
            {
                REQUIRE(Engine.ReadFrames(buffer.data(), CHUNK_FRAMES) == CHUNK_FRAMES);
                for (uint64_t i = 0; i < CHUNK_FRAMES; ++i)
                {
                    if (left)
                        *left += std::abs(buffer[i * 2]);
                    if (right)
                        *right += std::abs(buffer[i * 2 + 1]);
                }
            }
        }
    };
}

TEST_CASE("Audio clips - decoded once and cached by path")
{
    AudioWorld      world;
    HA::AudioClipId first = world.Beep();
    REQUIRE(first.IsValid());
    CHECK(world.Engine.GetClipLength(first) == doctest::Approx(0.1f));

    // A second load never reads the file again: it still loads with the file gone.
    std::filesystem::remove(world.Dir.Path() / "Audio/beep.wav");
    CHECK(world.Beep() == first);
}

TEST_CASE("Audio clips - a missing or undecodable file gives an invalid id and logs its path once")
{
    AudioWorld world;
    LogCapture log;

    CHECK_FALSE(world.Engine.LoadClip("assets://Audio/missing.wav", world.Files).IsValid());
    CHECK_FALSE(world.Engine.LoadClip("assets://Audio/missing.wav", world.Files).IsValid());
    CHECK(log.Lines("[Audio] assets://Audio/missing.wav: the file cannot be read.").size() == 1);

    CHECK_FALSE(world.Engine.LoadClip("assets://Audio/broken.wav", world.Files).IsValid());
    CHECK_FALSE(world.Engine.LoadClip("assets://Audio/broken.wav", world.Files).IsValid());
    CHECK(log.Lines("[Audio] assets://Audio/broken.wav: the file cannot be decoded").size() == 1);
    CHECK(log.Lines("[Audio]").size() == 2);

    // Neither plays, and nothing is left behind.
    CHECK_FALSE(world.Engine.Play(HA::AudioClipId{}, {}).IsValid());
    CHECK(world.Engine.GetActiveSoundCount() == 0);
    CHECK(world.Engine.GetClipLength(HA::AudioClipId{}) == 0.0f);
}

TEST_CASE("Audio playback - play, stop and IsPlaying")
{
    AudioWorld            world;
    const HA::SoundHandle sound = world.Engine.Play(world.Beep(), {});
    REQUIRE(sound.IsValid());
    CHECK(world.Engine.IsPlaying(sound));
    CHECK(world.Engine.GetActiveSoundCount() == 1);

    double left = 0.0;
    world.Mix(CHUNK_FRAMES, &left);
    CHECK(left > 0.0); // it is heard

    world.Engine.Stop(sound);
    CHECK_FALSE(world.Engine.IsPlaying(sound));
    CHECK(world.Engine.GetActiveSoundCount() == 0);

    double after = 0.0;
    world.Mix(CHUNK_FRAMES, &after);
    CHECK(after == 0.0);
}

TEST_CASE("Audio playback - a one-shot finishes and is reclaimed by Update; a loop keeps playing")
{
    AudioWorld            world;
    const HA::SoundHandle once = world.Engine.Play(world.Beep(), {});
    HA::PlayParams        looping;
    looping.Loop                 = true;
    const HA::SoundHandle loop   = world.Engine.Play(world.Beep(), looping);

    world.Mix(CLIP_FRAMES * 3);
    CHECK_FALSE(world.Engine.IsPlaying(once));
    CHECK(world.Engine.IsPlaying(loop));
    CHECK(world.Engine.GetActiveSoundCount() == 2); // the finished one waits for Update

    world.Engine.Update();
    CHECK(world.Engine.GetActiveSoundCount() == 1);
    CHECK(world.Engine.IsPlaying(loop));

    double left = 0.0;
    world.Mix(CHUNK_FRAMES, &left);
    CHECK(left > 0.0); // the loop is still heard after three clip lengths
}

TEST_CASE("Audio playback - a stale handle does nothing, even once its slot plays another sound")
{
    AudioWorld            world;
    const HA::SoundHandle first = world.Engine.Play(world.Beep(), {});
    world.Engine.Stop(first);
    const HA::SoundHandle second = world.Engine.Play(world.Beep(), {});
    CHECK(second.Index == first.Index); // the slot is reused...
    CHECK(second != first);             // ...under another generation

    world.Engine.Stop(first);
    world.Engine.SetVolume(first, 0.0f);
    world.Engine.SetPitch(first, 2.0f);
    world.Engine.SetSoundPose(first, HM::Vector3(100.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 0.0f, 0.0f));
    CHECK_FALSE(world.Engine.IsPlaying(first));
    CHECK(world.Engine.IsPlaying(second));

    double left = 0.0;
    world.Mix(CHUNK_FRAMES, &left);
    CHECK(left > 0.0); // the stale volume of 0 did not reach the new sound

    CHECK_FALSE(world.Engine.IsPlaying(HA::SoundHandle{}));
    world.Engine.Stop(HA::SoundHandle{});
}

TEST_CASE("Audio playback - StopAll stops every sound, and Play needs a running engine")
{
    AudioWorld     world;
    HA::PlayParams looping;
    looping.Loop = true;
    for (int i = 0; i < 4; ++i)
        REQUIRE(world.Engine.Play(world.Beep(), looping).IsValid());
    CHECK(world.Engine.GetActiveSoundCount() == 4);

    world.Engine.StopAll();
    CHECK(world.Engine.GetActiveSoundCount() == 0);
    double left = 0.0;
    world.Mix(CHUNK_FRAMES, &left);
    CHECK(left == 0.0);

    // Shutdown stops what plays, and the clip survives it.
    const HA::SoundHandle sound = world.Engine.Play(world.Beep(), looping);
    world.Engine.Shutdown();
    CHECK_FALSE(world.Engine.IsPlaying(sound));
    CHECK_FALSE(world.Engine.Play(world.Beep(), {}).IsValid());
    CHECK(world.Engine.GetClipLength(world.Beep()) == doctest::Approx(0.1f));
}

TEST_CASE("Audio playback - volume scales what is heard")
{
    AudioWorld world;
    double     full = 0.0, quiet = 0.0;
    {
        const HA::SoundHandle sound = world.Engine.Play(world.Beep(), {});
        world.Mix(CHUNK_FRAMES, &full);
        world.Engine.Stop(sound);
    }
    HA::PlayParams params;
    params.Volume = 0.25f;
    world.Engine.Play(world.Beep(), params);
    world.Mix(CHUNK_FRAMES, &quiet);
    CHECK(quiet == doctest::Approx(full * 0.25).epsilon(0.02));
}

TEST_CASE("Audio playback - a spatial sound to the listener's right is louder in the right channel")
{
    AudioWorld world;
    // The listener at the origin looking down -Z with +Y up, as a camera does: +X is its right.
    world.Engine.SetListenerPose(HM::Vector3(0.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 0.0f, -1.0f),
                                 HM::Vector3(0.0f, 1.0f, 0.0f));

    HA::PlayParams params;
    params.Spatial  = true;
    params.Position = HM::Vector3(3.0f, 0.0f, 0.0f);
    HA::SoundHandle sound = world.Engine.Play(world.Beep(), params);

    double left = 0.0, right = 0.0;
    world.Mix(CHUNK_FRAMES * 4, &left, &right);
    CHECK(right > left * 2.0);

    // Moved to the left, it swaps sides.
    world.Engine.SetSoundPose(sound, HM::Vector3(-3.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 0.0f, 0.0f));
    left = right = 0.0;
    world.Mix(CHUNK_FRAMES * 4, &left, &right);
    CHECK(left > right * 2.0);
    world.Engine.Stop(sound);

    // Not spatial, it is heard evenly wherever it is placed.
    params.Spatial = false;
    world.Engine.Play(world.Beep(), params);
    left = right = 0.0;
    world.Mix(CHUNK_FRAMES * 4, &left, &right);
    CHECK(left == doctest::Approx(right));
}
