#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"

#include "HedgehogAudio/api/AudioEngine.hpp"

#include <cmath>
#include <string>
#include <vector>

using HedgehogEngine::AudioSourceComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A script class named `name` with the given method bodies (empty ones are left out).
    std::string Script(const std::string& name, const std::string& onStart, const std::string& onUpdate = "")
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\n" + "function " + name + ":new() return setmetatable(ActorScript:new(), " + name +
                             ") end\n";
        if (!onStart.empty())
            source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onUpdate.empty())
            source += "function " + name + ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
        return source;
    }

    // The engine with its audio started without a device, so the test reads what it mixes. Clips
    // come from the engine's own Assets (Audio/Tone440.wav, a 0.5 s tone).
    struct AudioWorld : EngineWorld
    {
        AudioWorld()
        {
            HA::AudioEngineDesc desc;
            desc.NoDevice = true;
            REQUIRE(Audio().Init(desc));
        }

        HA::AudioEngine& Audio() { return Context.GetAudioEngine(); }

        // A whole engine frame, so AudioSystem places sounds and frees finished ones.
        void Step(int count = 1)
        {
            for (int i = 0; i < count; ++i)
                Context.UpdateContext(1.0f, STEP);
        }

        void Mix(uint64_t frames, double& left, double& right)
        {
            left = right = 0.0;
            std::vector<float> buffer(frames * 2);
            REQUIRE(Audio().ReadFrames(buffer.data(), frames) == frames);
            for (uint64_t i = 0; i < frames; ++i)
            {
                left += std::abs(buffer[i * 2]);
                right += std::abs(buffer[i * 2 + 1]);
            }
        }
    };
}

TEST_CASE("Audio bindings - a script starts and stops a source, and sets its volume and the master volume")
{
    AudioWorld world;
    world.WriteScript("Player.lua", Script("Player",
        "self.source = self.entity:addAudioSource()\n"
        "self.source.clip = 'Audio/Tone440.wav'\n"
        "self.source.loop = true\n"
        "self.source.volume = 0.25\n"
        "Log.info('started', self.source:play(), self.source:isPlaying())\n"
        "Audio.setMasterVolume(0.5)\n"
        "Log.info('master', Audio.getMasterVolume())",
        "self.frames = (self.frames or 0) + 1\n"
        "if self.frames == 2 then self.source:stop(); Log.info('stopped', self.source:isPlaying()) end"));
    const ECS::Entity entity = world.AddScripted("Scripts/Player.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step();
    CHECK(log.Lines("started true true").size() == 1);
    CHECK(log.Lines("master 0.5").size() == 1);
    CHECK(world.Audio().GetMasterVolume() == doctest::Approx(0.5f));

    const AudioSourceComponent& source = world.Ecs().GetComponent<AudioSourceComponent>(entity);
    CHECK(source.Clip == "Audio/Tone440.wav");
    CHECK(source.Volume == 0.25f);
    CHECK(source.Loop);
    CHECK(world.Audio().IsPlaying(source.Sound));

    world.Step();
    CHECK(log.Lines("stopped false").size() == 1);
    CHECK_FALSE(world.Ecs().GetComponent<AudioSourceComponent>(entity).Sound.IsValid());
    CHECK(world.Audio().GetActiveSoundCount() == 0);
    CHECK(log.Lines("stack traceback").empty()); // no script error
    REQUIRE(world.Stop());
}

TEST_CASE("Audio bindings - a positioned one-shot plays in 3D and is freed when it ends")
{
    AudioWorld world;
    world.WriteScript("Shot.lua", Script("Shot", "Log.info('shot', Audio.playOneShot('Audio/Tone440.wav', Vector3(3, 0, 0), 0.5))"));
    (void)world.AddScripted("Scripts/Shot.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step();
    CHECK(log.Lines("shot true").size() == 1);
    CHECK(world.Audio().GetActiveSoundCount() == 1);

    // Heard from the default listener at the origin looking down -Z: +X is on its right.
    double left = 0.0, right = 0.0;
    world.Mix(4800, left, right);
    CHECK(right > left * 2.0);

    world.Mix(48000, left, right); // past the clip's 0.5 s
    world.Step();
    CHECK(world.Audio().GetActiveSoundCount() == 0);
    REQUIRE(world.Stop());
}

TEST_CASE("Audio bindings - clips load only from under assets://, and bad values are script errors")
{
    AudioWorld world;
    world.WriteScript("Paths.lua", Script("Paths",
        "local function try(label, f) local ok, message = pcall(f); Log.info(label, ok, message) end\n"
        "try('drive', function() Audio.playOneShot('C:/Windows/Media/tada.wav') end)\n"
        "try('mount', function() Audio.playOneShot('engine://Assets/Audio/Tone440.wav') end)\n"
        "try('parent', function() Audio.playOneShot('Audio/../../secret.wav') end)\n"
        "try('rooted', function() Audio.playOneShot('/secret.wav') end)\n"
        "local source = self.entity:addAudioSource()\n"
        "try('clip', function() source.clip = 'shaders://x.wav' end)\n"
        "try('pitch', function() source.pitch = 0 end)\n"
        "try('position', function() Audio.playOneShot('Audio/Tone440.wav', 5) end)\n"
        "Log.info('missing', Audio.playOneShot('Audio/missing.wav'))\n"
        "Log.info('missing again', Audio.playOneShot('assets://Audio/missing.wav'))"));
    (void)world.AddScripted("Scripts/Paths.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step();
    CHECK(log.Lines("drive false").size() == 1);
    CHECK(log.Lines("audio clips load only from assets://, not 'C:/Windows/Media/tada.wav'").size() == 1);
    CHECK(log.Lines("mount false").size() == 1);
    CHECK(log.Lines("parent false").size() == 1);
    CHECK(log.Lines("may not leave assets://").size() == 1);
    CHECK(log.Lines("rooted false").size() == 1);
    CHECK(log.Lines("clip false").size() == 1);
    CHECK(log.Lines("pitch false").size() == 1);
    CHECK(log.Lines("a pitch must be above 0").size() == 1);
    CHECK(log.Lines("position false").size() == 1);
    CHECK(log.Lines("a one-shot's position must be a Vector3").size() == 1);
    // A clip that is not there is no error, only false, and the engine logs its path once.
    CHECK(log.Lines("missing false").size() == 1);
    CHECK(log.Lines("missing again false").size() == 1);
    CHECK(log.Lines("[Audio] assets://Audio/missing.wav: the file cannot be read.").size() == 1);
    CHECK(world.Audio().GetActiveSoundCount() == 0);
    REQUIRE(world.Stop());
}

TEST_CASE("Audio bindings - outside Play mode audio calls do nothing and the first one logs")
{
    AudioWorld world;
    // A script's top level runs when the editor describes it, in Edit mode.
    world.WriteScript("Loud.lua", "Audio.playOneShot('Audio/Tone440.wav')\n"
                                  "Audio.setMasterVolume(0.1)\n"
                                  "Audio.setBusVolume('Music', 0.1)\n" +
                                      Script("Loud", ""));

    LogCapture log;
    (void)world.Scripts->DescribeScript("assets://Scripts/Loud.lua");
    (void)world.Scripts->DescribeScript("assets://Scripts/Loud.lua");
    CHECK(world.Audio().GetActiveSoundCount() == 0);
    CHECK(world.Audio().GetMasterVolume() == doctest::Approx(1.0f));
    CHECK(world.Audio().GetBusVolume(HA::AudioBus::Music) == 1.0f);
    CHECK(log.Lines("[Script] Audio plays only in Play mode; Audio.playOneShot() did nothing.").size() == 1);
    CHECK(log.Lines("Audio plays only in Play mode").size() == 1);
}

TEST_CASE("Audio bindings - a bus volume of 0 silences its sources")
{
    // A looping non-spatial source on the Music bus, then `after`; returns what the engine mixes.
    const auto loudness = [](const std::string& after)
    {
        AudioWorld world;
        world.WriteScript("Buses.lua", Script("Buses",
            "local s = self.entity:addAudioSource()\n"
            "s.clip = 'Audio/Tone440.wav'; s.loop = true; s.spatial = false; s.bus = AudioBus.Music\n"
            "assert(s.bus == AudioBus.Music)\n"
            "s:play()\n" + after));
        (void)world.AddScripted("Scripts/Buses.lua");
        LogCapture log;
        REQUIRE(world.Context.Play());
        world.Step();
        CHECK(log.Lines("stack traceback").empty());
        double left = 0.0, right = 0.0;
        world.Mix(4800, left, right);
        REQUIRE(world.Stop());
        return left + right;
    };
    CHECK(loudness("") > 0.0);
    CHECK(loudness("Audio.setBusVolume('Music', 0)") == 0.0);
    CHECK(loudness("Audio.setBusVolume(AudioBus.Sfx, 0)") > 0.0);
    CHECK(loudness("Audio.setBusVolume('Master', 0)") == 0.0);
}

TEST_CASE("Audio bindings - bus volumes read back, one-shots take a bus, and bad buses are script errors")
{
    AudioWorld world;
    world.WriteScript("Volumes.lua", Script("Volumes",
        "local function try(label, f) local ok, message = pcall(f); Log.info(label, ok, message) end\n"
        "Audio.setBusVolume('Master', 0.5)\n"
        "Log.info('master', Audio.getBusVolume('Master') == Audio.getMasterVolume(), Audio.getMasterVolume())\n"
        "Audio.setBusVolume(AudioBus.Voice, 0.25)\n"
        "Log.info('voice', Audio.getBusVolume(AudioBus.Voice), Audio.getBusVolume('Voice'))\n"
        "local source = self.entity:addAudioSource()\n"
        "Log.info('bus', source.bus, math.type(source.bus))\n"
        "try('name', function() Audio.setBusVolume('Nope', 1) end)\n"
        "try('nan', function() Audio.setBusVolume('Music', 0/0) end)\n"
        "try('range', function() source.bus = 7 end)\n"
        "try('fraction', function() source.bus = 1.5 end)\n"
        "source.bus = 'Music'\n"
        "Log.info('named', source.bus == AudioBus.Music)\n"
        "try('table', function() Audio.setBusVolume({}, 1) end)\n"
        "try('number', function() Audio.getBusVolume(9) end)\n"
        "try('master shot', function() Audio.playOneShot('Audio/Tone440.wav', nil, nil, 'Master') end)\n"
        "Audio.setBusVolume('Music', 0)\n"
        "Log.info('muted shot', Audio.playOneShot('Audio/Tone440.wav', nil, nil, 'Music'))"));
    (void)world.AddScripted("Scripts/Volumes.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step();
    CHECK(log.Lines("master true 0.5").size() == 1);
    CHECK(log.Lines("voice 0.25 0.25").size() == 1);
    CHECK(log.Lines("bus 1 integer").size() == 1);
    CHECK(log.Lines("name false").size() == 1);
    CHECK(log.Lines("\"Nope\" is not an audio bus").size() == 1);
    CHECK(log.Lines("nan false").size() == 1);
    CHECK(log.Lines("range false").size() == 1);
    CHECK(log.Lines("7 is not an audio bus").size() == 1);
    CHECK(log.Lines("fraction false").size() == 1);
    CHECK(log.Lines("1.5 is not an audio bus").size() == 1);
    CHECK(log.Lines("named true").size() == 1);
    CHECK(log.Lines("table false").size() == 1);
    CHECK(log.Lines("number false").size() == 1);
    CHECK(log.Lines("master shot false").size() == 1);
    CHECK(log.Lines("a one-shot plays on a bus, not Master").size() == 1);
    CHECK(log.Lines("muted shot true").size() == 1);

    double left = 0.0, right = 0.0;
    world.Mix(4800, left, right);
    CHECK(left + right == 0.0); // the one-shot is on Music, which is at 0
    CHECK(world.Audio().GetBusVolume(HA::AudioBus::Music) == 0.0f);
    CHECK(world.Audio().GetBusVolume(HA::AudioBus::Voice) == doctest::Approx(0.25f));
    REQUIRE(world.Stop());
}
