#include "doctest/doctest/doctest.h"

#include "HedgehogAudio/api/AudioEngine.hpp"

#include "HedgehogScripting/tests/test_log_capture.hpp"

#include <limits>

namespace
{
    HA::AudioEngineDesc NoDevice()
    {
        HA::AudioEngineDesc desc;
        desc.NoDevice = true;
        return desc;
    }
}

TEST_CASE("AudioEngine - starts without a device and shuts down")
{
    HA::AudioEngine engine;
    CHECK_FALSE(engine.IsInitialized());
    CHECK(engine.GetChannels() == 0);

    REQUIRE(engine.Init(NoDevice()));
    CHECK(engine.IsInitialized());
    CHECK_FALSE(engine.HasDevice());
    CHECK(engine.GetChannels() == 2);
    CHECK(engine.GetSampleRate() == 48000);

    engine.Shutdown();
    CHECK_FALSE(engine.IsInitialized());
    CHECK(engine.GetSampleRate() == 0);
}

TEST_CASE("AudioEngine - shutting down twice, or before Init, does nothing")
{
    HA::AudioEngine engine;
    engine.Shutdown();
    REQUIRE(engine.Init(NoDevice()));
    engine.Shutdown();
    engine.Shutdown();
    CHECK_FALSE(engine.IsInitialized());

    // And it starts again after a shutdown, as Init does over a running engine.
    REQUIRE(engine.Init(NoDevice()));
    REQUIRE(engine.Init(NoDevice()));
    CHECK(engine.IsInitialized());
}

TEST_CASE("AudioEngine - a format of its own, and zeros falling back to stereo at 48 kHz")
{
    HA::AudioEngine     engine;
    HA::AudioEngineDesc desc = NoDevice();
    desc.Channels            = 1;
    desc.SampleRate          = 44100;
    REQUIRE(engine.Init(desc));
    CHECK(engine.GetChannels() == 1);
    CHECK(engine.GetSampleRate() == 44100);

    desc.Channels   = 0;
    desc.SampleRate = 0;
    REQUIRE(engine.Init(desc));
    CHECK(engine.GetChannels() == 2);
    CHECK(engine.GetSampleRate() == 48000);
}

TEST_CASE("AudioEngine - master volume is clamped to [0, 1] and kept across Init")
{
    HA::AudioEngine engine;
    CHECK(engine.GetMasterVolume() == doctest::Approx(1.0f));

    engine.SetMasterVolume(0.25f);
    REQUIRE(engine.Init(NoDevice()));
    CHECK(engine.GetMasterVolume() == doctest::Approx(0.25f)); // applied to the new engine

    engine.SetMasterVolume(3.0f);
    CHECK(engine.GetMasterVolume() == doctest::Approx(1.0f));
    engine.SetMasterVolume(-1.0f);
    CHECK(engine.GetMasterVolume() == doctest::Approx(0.0f));

    engine.SetMasterVolume(0.5f);
    engine.SetMasterVolume(std::numeric_limits<float>::quiet_NaN());
    engine.SetMasterVolume(std::numeric_limits<float>::infinity());
    CHECK(engine.GetMasterVolume() == doctest::Approx(0.5f));

    engine.Shutdown();
    CHECK(engine.GetMasterVolume() == doctest::Approx(0.5f));
}

TEST_CASE("AudioEngine - a device that cannot open falls back to a silent engine")
{
    // ALSA does not exist on Windows, so no device can open through it.
    HA::AudioEngine     engine;
    HA::AudioEngineDesc desc;
    desc.Backends = { HA::AudioBackend::Alsa };
    engine.SetMasterVolume(0.75f);

    LogCapture log;
    REQUIRE(engine.Init(desc));
    CHECK(log.Lines("[Audio] No output device could be opened").size() == 1);
    CHECK(log.Lines("[Audio]").size() == 1);
    CHECK(engine.IsInitialized());
    CHECK_FALSE(engine.HasDevice());
    CHECK(engine.GetChannels() == 2);
    CHECK(engine.GetMasterVolume() == doctest::Approx(0.75f));
    engine.Shutdown();
    CHECK_FALSE(engine.IsInitialized());
}
