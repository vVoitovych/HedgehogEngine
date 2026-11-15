#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/ECS/components/ParticleSystemComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/ParticleSystem.hpp"
#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Particles/ParticleEffectFile.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Time/FixedStepClock.hpp"

#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <memory>
#include <numeric>
#include <string>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    constexpr float FRAME = 1.0f / 60.0f;

    ParticleEmitter MakeEmitter(float rate)
    {
        ParticleEmitter emitter;
        emitter.Rate = rate;
        return emitter;
    }

    // The particles emitter spawns over frames frames of dt seconds each, from a fresh run.
    uint32_t SpawnOver(const ParticleEmitter& emitter, int frames, double dt = FRAME)
    {
        ParticleEmitterClock clock;
        uint32_t             total = 0;
        for (int frame = 0; frame < frames; ++frame)
            total += ParticleSystem::Advance(emitter, clock, dt);
        return total;
    }

    // An engine with particles:// on a temp folder holding Fire.particle (one emitter of rate 10
    // and Smoke, a second one of rate 30) and an entity playing it.
    struct ParticleWorld
    {
        TempDir       Dir;
        EngineContext Context;
        ECS::Entity   Entity = ECS::INVALID_ENTITY;

        ParticleWorld()
        {
            ParticleEffect effect = MakeDefaultParticleEffect();
            effect.Emitters[0].Name = "Fire";
            effect.Emitters[0].Rate = 10.0f;
            effect.Emitters.push_back(MakeEmitter(30.0f));
            effect.Emitters[1].Name = "Smoke";
            Dir.WriteFile("Fire.particle", WriteParticleEffect(effect));

            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("particles://", Dir.Path());
            REQUIRE(Context.GetFileSystem().Register(std::move(fs)));

            Entity = Context.GetSceneManager().CreateGameObject();
            ParticleSystemComponent particles;
            particles.Effect = "particles://Fire.particle";
            particles.Seed   = 7;
            Context.GetECS().AddComponent(Entity, particles);
        }

        ParticleSystem& System() { return *Context.GetParticleSystem(); }

        void Frames(int count)
        {
            for (int i = 0; i < count; ++i)
                Context.UpdateContext(1.0f, FRAME);
        }

        // The particles each emitter spawned over count frames.
        std::vector<uint32_t> Spawned(int count)
        {
            std::vector<uint32_t> totals;
            for (int i = 0; i < count; ++i)
            {
                Frames(1);
                const std::vector<ParticleFrameEmitter>& emitters = System().GetFrameEmitters();
                totals.resize(emitters.size());
                for (size_t e = 0; e < emitters.size(); ++e)
                    totals[e] += emitters[e].SpawnCount;
            }
            return totals;
        }
    };
}

TEST_CASE("Particle spawning - a rate spawns its whole particles and carries the fraction")
{
    CHECK(SpawnOver(MakeEmitter(10.0f), 60) == 10);
    CHECK(SpawnOver(MakeEmitter(2.5f), 60) == 2);   // 2.5 owed, the half carried
    CHECK(SpawnOver(MakeEmitter(2.5f), 120) == 5);
    CHECK(SpawnOver(MakeEmitter(1000.0f), 1) == 16); // 16.67 in one frame
    CHECK(SpawnOver(MakeEmitter(0.0f), 600) == 0);
    CHECK(SpawnOver(MakeEmitter(10.0f), 60, 0.0) == 0); // no time, no particles
}

TEST_CASE("Particle spawning - bursts fire at their time, repeat by cycle and re-arm on loop")
{
    ParticleEmitter emitter = MakeEmitter(0.0f);
    emitter.Duration        = 2.0f;
    emitter.Bursts          = { { 0.0f, 5, 1, 1.0f } };
    // Once at the start of each 2 s run: 3 runs in 6 s.
    CHECK(SpawnOver(emitter, 1) == 5);
    CHECK(SpawnOver(emitter, 359) == 15);

    // Three firings half a second apart, from 0.25 s, then not again until the run starts over.
    emitter.Bursts = { { 0.25f, 4, 3, 0.5f } };
    CHECK(SpawnOver(emitter, 14) == 0);  // to 0.233 s, not yet
    CHECK(SpawnOver(emitter, 16) == 4);  // 0.25
    CHECK(SpawnOver(emitter, 46) == 8);  // 0.25, 0.75
    CHECK(SpawnOver(emitter, 120) == 12);
    CHECK(SpawnOver(emitter, 240) == 24); // two runs

    // Cycles 0 repeats for as long as the run lasts: 0, 0.5, 1, 1.5 (and 0 again once it starts over).
    emitter.Bursts = { { 0.0f, 1, 0, 0.5f } };
    CHECK(SpawnOver(emitter, 119) == 4);
    CHECK(SpawnOver(emitter, 1, 2.25) == 5);

    // A frame longer than the run wraps it, firing each run's burst.
    emitter.Bursts = { { 0.0f, 1, 1, 1.0f } };
    CHECK(SpawnOver(emitter, 1, 5.0) == 3); // runs starting at 0, 2 and 4 s
}

TEST_CASE("Particle spawning - a run that does not loop stops at its duration")
{
    ParticleEmitter emitter = MakeEmitter(10.0f);
    emitter.Duration        = 1.0f;
    emitter.Loop            = false;
    emitter.Bursts          = { { 0.5f, 3, 1, 1.0f } };

    ParticleEmitterClock clock;
    uint32_t             total = 0;
    for (int frame = 0; frame < 180; ++frame)
        total += ParticleSystem::Advance(emitter, clock, FRAME);
    CHECK(total == 10 + 3);
    CHECK(clock.Finished);
    CHECK(ParticleSystem::Advance(emitter, clock, FRAME) == 0);

    // No duration and no loop: nothing.
    emitter.Duration = 0.0f;
    CHECK(SpawnOver(emitter, 60) == 0);
}

TEST_CASE("Particle system - spawns only in Play, at the frame's scaled time, and Stop restores the scene")
{
    ParticleWorld world;
    const std::string before = world.Context.GetSceneManager().CaptureSnapshot().Yaml;

    // Edit: listed, nothing spawned.
    CHECK(world.Spawned(60) == std::vector<uint32_t>{ 0, 0 });
    const std::vector<ParticleFrameEmitter>& emitters = world.System().GetFrameEmitters();
    REQUIRE(emitters.size() == 2);
    CHECK(emitters[0].Entity == world.Entity);
    CHECK(emitters[0].EmitterIndex == 0);
    CHECK(emitters[1].EmitterIndex == 1);
    CHECK_FALSE(emitters[0].Playing);
    CHECK(emitters[0].DeltaTime == 0.0f);

    REQUIRE(world.Context.Play());
    CHECK(world.System().IsPlaying(world.Context.GetECS(), world.Entity));
    CHECK(world.Spawned(60) == std::vector<uint32_t>{ 10, 30 });
    CHECK(world.System().GetFrameEmitters()[0].Playing);
    CHECK(world.System().GetFrameEmitters()[0].DeltaTime == doctest::Approx(FRAME));

    // Paused holds.
    REQUIRE(world.Context.Pause());
    CHECK(world.Spawned(60) == std::vector<uint32_t>{ 0, 0 });
    REQUIRE(world.Context.Resume());

    // Half the time scale, half the particles.
    world.Context.GetFixedStepClock().TimeScale = 0.5f;
    CHECK(world.Spawned(120) == std::vector<uint32_t>{ 10, 30 });

    REQUIRE(world.Context.Stop());
    CHECK(world.Context.GetSceneManager().CaptureSnapshot().Yaml == before);
    CHECK_FALSE(world.System().IsPlaying(world.Context.GetECS(), world.Entity));
}

TEST_CASE("Particle system - Emit, Stop and Play, and PlayOnStart off")
{
    ParticleWorld world;
    ECS::ECS&     ecs = world.Context.GetECS();
    ecs.GetComponent<ParticleSystemComponent>(world.Entity).PlayOnStart = false;

    REQUIRE(world.Context.Play());
    CHECK_FALSE(world.System().IsPlaying(ecs, world.Entity));
    CHECK(world.Spawned(60) == std::vector<uint32_t>{ 0, 0 });

    // Emit adds to the next frame of every emitter, playing or not.
    world.System().Emit(ecs, world.Entity, 5);
    CHECK(world.Spawned(1) == std::vector<uint32_t>{ 5, 5 });
    CHECK(world.Spawned(1) == std::vector<uint32_t>{ 0, 0 });

    const uint32_t epoch = ecs.GetComponent<ParticleSystemComponent>(world.Entity).Epoch;
    world.System().Play(ecs, world.Entity);
    CHECK(ecs.GetComponent<ParticleSystemComponent>(world.Entity).Epoch == epoch + 1);
    CHECK(world.Spawned(60) == std::vector<uint32_t>{ 10, 30 });

    // Stop keeps what is alive; Stop with clear starts a new epoch.
    world.System().Stop(ecs, world.Entity, false);
    CHECK(ecs.GetComponent<ParticleSystemComponent>(world.Entity).Epoch == epoch + 1);
    CHECK(world.Spawned(60) == std::vector<uint32_t>{ 0, 0 });
    world.System().Stop(ecs, world.Entity, true);
    CHECK(ecs.GetComponent<ParticleSystemComponent>(world.Entity).Epoch == epoch + 2);

    // A disabled one is not listed.
    ecs.GetComponent<ParticleSystemComponent>(world.Entity).Enabled = false;
    world.Frames(1);
    CHECK(world.System().GetFrameEmitters().empty());
}

TEST_CASE("Particle system - the same seed and frames give the same list, another seed another")
{
    const auto run = [](uint32_t seed)
    {
        ParticleWorld world;
        world.Context.GetECS().GetComponent<ParticleSystemComponent>(world.Entity).Seed = seed;
        REQUIRE(world.Context.Play());
        std::vector<uint32_t> seeds;
        for (int frame = 0; frame < 10; ++frame)
        {
            world.Frames(1);
            for (const ParticleFrameEmitter& emitter : world.System().GetFrameEmitters())
                seeds.push_back(emitter.Seed);
        }
        return seeds;
    };
    const std::vector<uint32_t> first = run(7);
    CHECK(first == run(7));
    CHECK(first != run(8));
    // Every entry differs from the others: by emitter and by frame.
    std::vector<uint32_t> sorted = first;
    std::sort(sorted.begin(), sorted.end());
    CHECK(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end());
}

TEST_CASE("Particle system - an effect that does not load is not listed")
{
    ParticleWorld world;
    world.Context.GetECS().GetComponent<ParticleSystemComponent>(world.Entity).Effect = "particles://Missing.particle";
    REQUIRE(world.Context.Play());
    world.Frames(1);
    CHECK(world.System().GetFrameEmitters().empty());
    CHECK(world.Context.GetECS().GetComponent<ParticleSystemComponent>(world.Entity).EffectIndex == -1);
}
