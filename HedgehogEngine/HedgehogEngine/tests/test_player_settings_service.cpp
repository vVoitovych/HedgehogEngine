#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/HedgehogSettings/api/PlayerSettings.hpp"

#include "HedgehogAudio/api/AudioEngine.hpp"

using HedgehogEngine::EngineContext;
using HedgehogSettings::PlayerSettings;

TEST_CASE("Player settings - an ECS service the engine keeps and flags")
{
    EngineContext context;
    CHECK(context.GetPlayerSettings() == PlayerSettings{});
    CHECK(context.GetECS().GetServices().Find<PlayerSettings>() == &context.GetPlayerSettings());

    const uint64_t version = context.GetPlayerSettingsVersion();
    context.GetPlayerSettings().Quality = HedgehogSettings::PlayerQuality::Low;
    CHECK(context.GetPlayerSettingsVersion() == version); // a change counts once it is marked
    context.MarkPlayerSettingsChanged();
    CHECK(context.GetPlayerSettingsVersion() == version + 1);

    // Play and Stop leave the player's choices alone.
    REQUIRE(context.Play());
    REQUIRE(context.Stop());
    CHECK(context.GetPlayerSettings().Quality == HedgehogSettings::PlayerQuality::Low);
}

TEST_CASE("Player settings - ApplyPlayerVolumes sets the master and every bus")
{
    EngineContext       context;
    HA::AudioEngineDesc desc;
    desc.NoDevice = true;
    REQUIRE(context.GetAudioEngine().Init(desc));

    PlayerSettings& player = context.GetPlayerSettings();
    player.MasterVolume    = 0.5f;
    player.BusVolumes      = { 0.2f, 0.4f, 0.6f, 0.8f };
    context.ApplyPlayerVolumes();

    HA::AudioEngine& audio = context.GetAudioEngine();
    CHECK(audio.GetMasterVolume() == doctest::Approx(0.5f));
    CHECK(audio.GetBusVolume(HA::AudioBus::Music) == doctest::Approx(0.2f));
    CHECK(audio.GetBusVolume(HA::AudioBus::Sfx) == doctest::Approx(0.4f));
    CHECK(audio.GetBusVolume(HA::AudioBus::Voice) == doctest::Approx(0.6f));
    CHECK(audio.GetBusVolume(HA::AudioBus::Ui) == doctest::Approx(0.8f));
}
