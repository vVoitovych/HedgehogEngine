#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/ECS/components/AudioListenerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/AudioSourceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/CameraComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/AudioSystem.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "HedgehogAudio/api/AudioEngine.hpp"

#include "ECS/api/ECS.hpp"

#include <cmath>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    constexpr float    STEP   = 1.0f / 60.0f;
    constexpr uint64_t FRAMES = 4800; // 0.1 s at 48 kHz

    // An engine whose audio runs without a device, so the test reads what it mixes. The shipped
    // Assets/Audio/Tone440.wav is a 0.5 s tone.
    struct AudioWorld
    {
        EngineContext Context;

        AudioWorld()
        {
            HA::AudioEngineDesc desc;
            desc.NoDevice = true;
            REQUIRE(Context.GetAudioEngine().Init(desc));
        }

        ECS::ECS&        Ecs() { return Context.GetECS(); }
        HA::AudioEngine& Audio() { return Context.GetAudioEngine(); }

        ECS::Entity Spawn(const HM::Vector3& position, const HM::Vector3& rotation = HM::Vector3(0.0f, 0.0f, 0.0f))
        {
            const ECS::Entity entity = Context.GetSceneManager().CreateGameObject();
            Move(entity, position, rotation);
            return entity;
        }

        void Move(ECS::Entity entity, const HM::Vector3& position, const HM::Vector3& rotation)
        {
            auto& transform    = Ecs().GetComponent<TransformComponent>(entity);
            transform.Position = position;
            transform.Rotation = rotation;
            Context.GetEventBus().Publish(TransformChangedEvent{ entity });
        }

        ECS::Entity SpawnSource(const HM::Vector3& position, bool loop = true, bool playOnStart = true)
        {
            const ECS::Entity    entity = Spawn(position);
            AudioSourceComponent source;
            source.Clip        = "Audio/Tone440.wav";
            source.Loop        = loop;
            source.PlayOnStart = playOnStart;
            Ecs().AddComponent(entity, source);
            return entity;
        }

        const AudioSourceComponent& Source(ECS::Entity entity) { return Ecs().GetComponent<AudioSourceComponent>(entity); }

        void Frame() { Context.UpdateContext(1.0f, STEP); }

        // Mixes frames and sums each channel's magnitude.
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

        double Loudness()
        {
            double left = 0.0, right = 0.0;
            Mix(FRAMES, left, right);
            return left + right;
        }
    };

    const HM::Vector3 RIGHT(3.0f, 0.0f, 0.0f);
    const HM::Vector3 TURNED_AROUND(0.0f, 180.0f, 0.0f); // facing +Z: +X is on the left
}

TEST_CASE("AudioSystem - silent in Edit; Play starts PlayOnStart sources, Pause holds, Stop silences")
{
    AudioWorld        world;
    const ECS::Entity looping = world.SpawnSource(RIGHT);
    const ECS::Entity waiting = world.SpawnSource(RIGHT, true, false);

    world.Frame();
    CHECK(world.Audio().GetActiveSoundCount() == 0);
    CHECK(world.Loudness() == 0.0);

    REQUIRE(world.Context.Play());
    world.Frame();
    CHECK(world.Audio().IsPlaying(world.Source(looping).Sound));
    CHECK_FALSE(world.Source(waiting).Sound.IsValid());
    CHECK(world.Audio().GetActiveSoundCount() == 1);
    CHECK(world.Loudness() > 0.0);

    REQUIRE(world.Context.Pause());
    world.Frame();
    CHECK_FALSE(world.Audio().IsPlaying(world.Source(looping).Sound));
    CHECK(world.Audio().Exists(world.Source(looping).Sound)); // held, not stopped
    CHECK(world.Loudness() == 0.0);

    REQUIRE(world.Context.Resume());
    world.Frame();
    CHECK(world.Audio().IsPlaying(world.Source(looping).Sound));
    CHECK(world.Loudness() > 0.0);

    REQUIRE(world.Context.Stop());
    world.Frame();
    CHECK(world.Audio().GetActiveSoundCount() == 0);
    CHECK_FALSE(world.Source(looping).Sound.IsValid());
    CHECK(world.Loudness() == 0.0);
}

TEST_CASE("AudioSystem - a source pans with its place relative to the listener")
{
    AudioWorld        world;
    const ECS::Entity source = world.SpawnSource(RIGHT);
    REQUIRE(world.Context.Play());
    world.Frame();

    // No listener and no camera: heard from the origin, looking down -Z.
    double left = 0.0, right = 0.0;
    world.Mix(FRAMES, left, right);
    CHECK(right > left * 2.0);

    // Moved to the left during Play, it follows.
    world.Move(source, HM::Vector3(-3.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 0.0f, 0.0f));
    world.Frame();
    world.Mix(FRAMES, left, right);
    CHECK(left > right * 2.0);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("AudioSystem - the first active listener hears it, else the camera")
{
    AudioWorld        world;
    const ECS::Entity source = world.SpawnSource(RIGHT);
    (void)source;

    // A camera turned around: +X is on its left.
    const ECS::Entity camera = world.Spawn(HM::Vector3(0.0f, 0.0f, 0.0f), TURNED_AROUND);
    world.Ecs().AddComponent(camera, CameraComponent{});
    REQUIRE(world.Context.Play());
    world.Frame();
    double left = 0.0, right = 0.0;
    world.Mix(FRAMES, left, right);
    CHECK(left > right * 2.0);

    // A listener facing -Z takes over from the camera; an inactive one does not.
    const ECS::Entity inactive = world.Spawn(HM::Vector3(0.0f, 0.0f, 0.0f), TURNED_AROUND);
    AudioListenerComponent off;
    off.IsActive = false;
    world.Ecs().AddComponent(inactive, off);
    const ECS::Entity listener = world.Spawn(HM::Vector3(0.0f, 0.0f, 0.0f));
    world.Ecs().AddComponent(listener, AudioListenerComponent{});
    world.Frame();
    world.Mix(FRAMES, left, right);
    CHECK(right > left * 2.0);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("AudioSystem - a removed source stops, and a finished one-shot is forgotten")
{
    AudioWorld        world;
    const ECS::Entity looping = world.SpawnSource(RIGHT);
    const ECS::Entity once    = world.SpawnSource(RIGHT, false);
    REQUIRE(world.Context.Play());
    world.Frame();
    REQUIRE(world.Audio().GetActiveSoundCount() == 2);

    const HA::SoundHandle loopSound = world.Source(looping).Sound;
    world.Ecs().RemoveComponent<AudioSourceComponent>(looping);
    CHECK_FALSE(world.Audio().Exists(loopSound));

    // The 0.5 s clip plays out, and the next frame releases it.
    double left = 0.0, right = 0.0;
    world.Mix(48000, left, right);
    world.Frame();
    CHECK_FALSE(world.Source(once).Sound.IsValid());
    CHECK(world.Audio().GetActiveSoundCount() == 0);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("AudioSystem - an engine that was never started plays nothing and fails nothing")
{
    EngineContext context;
    auto&         ecs    = context.GetECS();
    const auto    entity = context.GetSceneManager().CreateGameObject();
    AudioSourceComponent source;
    source.Clip = "Audio/Tone440.wav";
    ecs.AddComponent(entity, source);

    REQUIRE(context.Play());
    context.UpdateContext(1.0f, STEP);
    CHECK_FALSE(ecs.GetComponent<AudioSourceComponent>(entity).Sound.IsValid());
    REQUIRE(context.Stop());
}

TEST_CASE("AudioSystem - runs in the Late phase and takes its services from the ECS")
{
    AudioWorld world;
    CHECK(world.Context.GetAudioSystem()->GetPhase() == ECS::SystemPhase::Late);

    // A source moved in a frame is heard from its new place in that frame: Audio runs in Late,
    // after the Transform phase has built the world matrices.
    const ECS::Entity source = world.SpawnSource(RIGHT);
    REQUIRE(world.Context.Play());
    world.Frame();
    world.Move(source, HM::Vector3(-3.0f, 0.0f, 0.0f), HM::Vector3(0.0f, 0.0f, 0.0f));
    world.Frame();
    double left = 0.0, right = 0.0;
    world.Mix(FRAMES, left, right);
    CHECK(left > right);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("AudioSystem - once unregistered, removing a source no longer reaches it")
{
    AudioWorld        world;
    const ECS::Entity source = world.SpawnSource(RIGHT);
    REQUIRE(world.Context.Play());
    world.Frame();
    const HA::SoundHandle sound = world.Source(source).Sound;
    REQUIRE(world.Audio().IsPlaying(sound));

    REQUIRE(world.Ecs().UnregisterSystem<AudioSystem>());
    world.Ecs().RemoveComponent<AudioSourceComponent>(source);
    // The removal callback went with the system, so nothing stopped the sound.
    CHECK(world.Audio().IsPlaying(sound));
    world.Audio().StopAll();
}

TEST_CASE("AudioSystem - without an audio engine service it registers and plays nothing")
{
    ECS::ECS ecs;
    ecs.Init();
    ecs.RegisterComponent<TransformComponent>();
    ecs.RegisterComponent<AudioSourceComponent>();
    auto system = ecs.RegisterSystem<AudioSystem>();

    const ECS::Entity entity = ecs.CreateEntity();
    ecs.AddComponent(entity, TransformComponent{});
    ecs.AddComponent(entity, AudioSourceComponent{});
    CHECK_FALSE(system->Play(ecs, entity).IsValid());
    ecs.NotifyPlayStart();
    ECS::FrameContext ctx;
    ecs.RunPhase(ECS::SystemPhase::Late, ctx);
    ecs.RemoveComponent<AudioSourceComponent>(entity);
    CHECK(ecs.UnregisterSystem<AudioSystem>());
}
