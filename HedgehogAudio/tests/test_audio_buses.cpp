#include "doctest/doctest/doctest.h"

#include "test_audio_world.hpp"

#include <cmath>
#include <limits>

namespace
{
    // The summed magnitude of one sound of the beep on a bus, played alone for a clip's length.
    double Heard(AudioWorld& world, HA::AudioBus bus)
    {
        HA::PlayParams params;
        params.Bus = bus;
        const HA::SoundHandle sound = world.Engine.Play(world.Beep(), params);
        REQUIRE(sound.IsValid());
        double left = 0.0;
        world.Mix(CLIP_FRAMES, &left);
        world.Engine.Stop(sound);
        return left;
    }
}

TEST_CASE("Audio buses - defaults")
{
    AudioWorld world;
    CHECK(HA::PlayParams{}.Bus == HA::AudioBus::Sfx);
    for (size_t i = 0; i < HA::AUDIO_BUS_COUNT; ++i)
    {
        CHECK(world.Engine.GetBusVolume(static_cast<HA::AudioBus>(i)) == 1.0f);
    }
}

TEST_CASE("Audio buses - a bus scales only its own sounds")
{
    AudioWorld   world;
    const double music = Heard(world, HA::AudioBus::Music);
    const double sfx   = Heard(world, HA::AudioBus::Sfx);
    REQUIRE(music > 0.0);

    world.Engine.SetBusVolume(HA::AudioBus::Music, 0.5f);
    CHECK(Heard(world, HA::AudioBus::Music) == doctest::Approx(music * 0.5).epsilon(0.02));
    CHECK(Heard(world, HA::AudioBus::Sfx) == doctest::Approx(sfx).epsilon(0.02));

    world.Engine.SetBusVolume(HA::AudioBus::Music, 0.0f);
    CHECK(Heard(world, HA::AudioBus::Music) == doctest::Approx(0.0).epsilon(0.001));
}

TEST_CASE("Audio buses - master times bus")
{
    AudioWorld   world;
    const double full = Heard(world, HA::AudioBus::Ui);
    world.Engine.SetMasterVolume(0.5f);
    world.Engine.SetBusVolume(HA::AudioBus::Ui, 0.5f);
    CHECK(Heard(world, HA::AudioBus::Ui) == doctest::Approx(full * 0.25).epsilon(0.02));
}

TEST_CASE("Audio buses - a change applies to a sound already playing")
{
    AudioWorld            world;
    const HA::SoundHandle sound = world.Engine.Play(world.Beep(), { .Loop = true, .Bus = HA::AudioBus::Voice });
    REQUIRE(sound.IsValid());
    double before = 0.0;
    world.Mix(CLIP_FRAMES, &before);
    world.Engine.SetBusVolume(HA::AudioBus::Voice, 0.5f);
    double after = 0.0;
    world.Mix(CLIP_FRAMES, &after);
    CHECK(after == doctest::Approx(before * 0.5).epsilon(0.02));
}

TEST_CASE("Audio buses - clamped, and non-finite values ignored")
{
    AudioWorld world;
    world.Engine.SetBusVolume(HA::AudioBus::Music, -1.0f);
    CHECK(world.Engine.GetBusVolume(HA::AudioBus::Music) == 0.0f);
    world.Engine.SetBusVolume(HA::AudioBus::Music, 2.0f);
    CHECK(world.Engine.GetBusVolume(HA::AudioBus::Music) == 1.0f);

    world.Engine.SetBusVolume(HA::AudioBus::Music, 0.25f);
    world.Engine.SetBusVolume(HA::AudioBus::Music, std::numeric_limits<float>::quiet_NaN());
    world.Engine.SetBusVolume(HA::AudioBus::Music, std::numeric_limits<float>::infinity());
    CHECK(world.Engine.GetBusVolume(HA::AudioBus::Music) == 0.25f);

    world.Engine.SetBusVolume(static_cast<HA::AudioBus>(9), 0.5f);
    CHECK(world.Engine.GetBusVolume(static_cast<HA::AudioBus>(9)) == 0.0f);
}

TEST_CASE("Audio buses - volumes kept across Init and set before it")
{
    HA::AudioEngine engine;
    engine.SetBusVolume(HA::AudioBus::Ui, 0.5f); // before Init
    CHECK(engine.GetBusVolume(HA::AudioBus::Ui) == 0.5f);

    AudioWorld world;
    const double full = Heard(world, HA::AudioBus::Ui);
    world.Engine.SetBusVolume(HA::AudioBus::Ui, 0.5f);
    world.Engine.Shutdown();
    HA::AudioEngineDesc desc;
    desc.NoDevice = true;
    REQUIRE(world.Engine.Init(desc));
    CHECK(world.Engine.GetBusVolume(HA::AudioBus::Ui) == 0.5f);
    CHECK(Heard(world, HA::AudioBus::Ui) == doctest::Approx(full * 0.5).epsilon(0.02));
}

TEST_CASE("Audio buses - names round trip")
{
    const char* names[] = { "Music", "Sfx", "Voice", "Ui" };
    for (size_t i = 0; i < HA::AUDIO_BUS_COUNT; ++i)
    {
        const HA::AudioBus bus = static_cast<HA::AudioBus>(i);
        CHECK(HA::GetBusName(bus) == names[i]);
        REQUIRE(HA::FindBus(HA::GetBusName(bus)).has_value());
        CHECK(*HA::FindBus(HA::GetBusName(bus)) == bus);
    }
    CHECK(HA::GetBusName(static_cast<HA::AudioBus>(9)).empty());
    CHECK_FALSE(HA::FindBus("sfx").has_value());
    CHECK_FALSE(HA::FindBus("").has_value());
    CHECK_FALSE(HA::FindBus("Master").has_value());
}
