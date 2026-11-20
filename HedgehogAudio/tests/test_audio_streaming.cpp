#include "doctest/doctest/doctest.h"

#include "test_audio_world.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace
{
    constexpr uint32_t MUSIC_FRAMES = RATE * 3; // 3 s: three of the resource manager's 1 s pages
    const std::string  MUSIC        = "assets://Audio/music.wav";

    // Mixes frames (a whole number of chunks) and returns the left channel.
    std::vector<float> Record(HA::AudioEngine& engine, uint64_t frames)
    {
        std::vector<float> left;
        std::vector<float> buffer(CHUNK_FRAMES * 2);
        for (uint64_t done = 0; done < frames; done += CHUNK_FRAMES)
        {
            REQUIRE(engine.ReadFrames(buffer.data(), CHUNK_FRAMES) == CHUNK_FRAMES);
            for (uint64_t i = 0; i < CHUNK_FRAMES; ++i)
                left.push_back(buffer[i * 2]);
        }
        return left;
    }

    double Loudness(const std::vector<float>& samples, size_t first, size_t count)
    {
        double sum = 0.0;
        for (size_t i = first; i < first + count; ++i)
            sum += std::abs(samples[i]);
        return sum;
    }
}

TEST_CASE("Audio streaming - a streamed WAV mixes the samples its decoded clip does, and caches nothing")
{
    AudioWorld world;
    world.Dir.WriteFile("Audio/music.wav", MakeWav(MUSIC_FRAMES));

    const HA::SoundHandle stream = world.Engine.PlayStream(MUSIC, world.Files, {});
    REQUIRE(stream.IsValid());
    CHECK(world.Engine.GetCachedClipCount() == 0);
    const std::vector<float> streamed = Record(world.Engine, MUSIC_FRAMES);
    world.Engine.Stop(stream);

    const HA::AudioClipId clip = world.Engine.LoadClip(MUSIC, world.Files);
    REQUIRE(clip.IsValid());
    CHECK(world.Engine.GetCachedClipCount() == 1);
    const HA::SoundHandle    sound   = world.Engine.Play(clip, {});
    const std::vector<float> decoded = Record(world.Engine, MUSIC_FRAMES);
    world.Engine.Stop(sound);

    REQUIRE(streamed.size() == decoded.size());
    size_t differing = 0;
    for (size_t i = 0; i < streamed.size(); ++i)
        differing += std::abs(streamed[i] - decoded[i]) > 1e-4f ? 1 : 0;
    CHECK(differing == 0);
    CHECK(Loudness(streamed, streamed.size() - RATE, RATE) > 1000.0); // its last second is heard too
}

TEST_CASE("Audio streaming - a looping stream plays past its end; Stop makes its handle stale")
{
    AudioWorld world;
    world.Dir.WriteFile("Audio/music.wav", MakeWav(MUSIC_FRAMES));

    HA::PlayParams looping;
    looping.Loop                 = true;
    const HA::SoundHandle stream = world.Engine.PlayStream(MUSIC, world.Files, looping);
    REQUIRE(stream.IsValid());
    const std::vector<float> samples = Record(world.Engine, MUSIC_FRAMES + RATE);
    CHECK(Loudness(samples, MUSIC_FRAMES, RATE) > 1000.0);
    world.Engine.Update();
    CHECK(world.Engine.IsPlaying(stream));

    world.Engine.Stop(stream);
    CHECK_FALSE(world.Engine.Exists(stream));
    CHECK(Loudness(Record(world.Engine, CHUNK_FRAMES), 0, CHUNK_FRAMES) == 0.0);

    // Its slot plays another stream under a new generation; the old handle reaches nothing.
    const HA::SoundHandle next = world.Engine.PlayStream(MUSIC, world.Files, {});
    REQUIRE(next.IsValid());
    CHECK(next.Index == stream.Index);
    world.Engine.SetVolume(stream, 0.0f);
    world.Engine.Stop(stream);
    CHECK(world.Engine.IsPlaying(next));

    // A one-shot stream ends and Update reclaims it.
    Record(world.Engine, MUSIC_FRAMES + CHUNK_FRAMES * 4);
    CHECK_FALSE(world.Engine.IsPlaying(next));
    world.Engine.Update();
    CHECK(world.Engine.GetActiveSoundCount() == 0);
}

TEST_CASE("Audio streaming - a missing file logs once and gives an invalid handle")
{
    AudioWorld world;
    LogCapture log;
    CHECK_FALSE(world.Engine.PlayStream("assets://Audio/missing.wav", world.Files, {}).IsValid());
    CHECK_FALSE(world.Engine.PlayStream("assets://Audio/missing.wav", world.Files, {}).IsValid());
    CHECK(log.Lines("[Audio] assets://Audio/missing.wav cannot be streamed").size() == 1);

    CHECK_FALSE(world.Engine.PlayStream("assets://Audio/broken.wav", world.Files, {}).IsValid());
    CHECK(log.Lines("[Audio] assets://Audio/broken.wav cannot be streamed").size() == 1);
    CHECK(world.Engine.GetActiveSoundCount() == 0);

    HA::AudioEngine stopped;
    CHECK_FALSE(stopped.PlayStream("assets://Audio/beep.wav", world.Files, {}).IsValid());
}
