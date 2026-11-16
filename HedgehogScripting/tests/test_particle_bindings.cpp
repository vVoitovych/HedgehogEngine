#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "ECS/api/components/Hierarchy.hpp"
#include "HedgehogEngine/api/ECS/components/ParticleSystemComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/ParticleSystem.hpp"

#include <string>
#include <vector>

using HedgehogEngine::ParticleSystemComponent;

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

    // The engine's own assets:// (the sample project's) holds Particles/Fire.particle, one emitter
    // of 60 particles a second, so every 1/60 s frame it plays spawns one.
    struct ParticleWorld : EngineWorld
    {
        // A whole engine frame, so the particle system (in the Late phase) counts its spawns.
        uint32_t Step()
        {
            Context.UpdateContext(1.0f, STEP);
            uint32_t spawned = 0;
            for (const HedgehogEngine::ParticleFrameEmitter& emitter : Context.GetParticleSystem()->GetFrameEmitters())
                spawned += emitter.SpawnCount;
            return spawned;
        }
    };
}

TEST_CASE("Particle bindings - a script adds a particle system, bursts it, plays and stops it")
{
    ParticleWorld world;
    world.WriteScript("Fireworks.lua", Script("Fireworks",
        "self.particles = self.entity:addParticleSystem()\n"
        "self.particles.effect = 'assets://Particles/Fire.particle'\n"
        "Log.info('effect', self.particles.effect, self.particles:isPlaying())",
        "self.frame = (self.frame or 0) + 1\n"
        "if self.frame == 2 then self.particles:emit(5) end\n"
        "if self.frame == 4 then self.particles:play() end\n"
        "if self.frame == 6 then self.particles:stop() end\n"
        "Log.info('frame', self.frame, self.particles:isPlaying())"));
    const ECS::Entity entity = world.AddScripted("Scripts/Fireworks.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    // An added system does not start on its own; emit(5) adds exactly 5 to that frame's spawns.
    CHECK(world.Step() == 0);
    CHECK(log.Lines("effect Particles/Fire.particle false").size() == 1);
    CHECK(world.Step() == 5);
    CHECK(world.Step() == 0);
    // play() spawns one a frame until stop().
    CHECK(world.Step() == 1);
    CHECK(log.Lines("frame 4 true").size() == 1);
    CHECK(world.Step() == 1);
    CHECK(world.Step() == 0);
    CHECK(log.Lines("frame 6 false").size() == 1);
    CHECK_FALSE(world.Context.GetParticleSystem()->IsPlaying(world.Ecs(), entity));

    REQUIRE(world.Stop());
    CHECK_FALSE(world.Ecs().HasComponent<ParticleSystemComponent>(entity)); // added in Play, gone at Stop
}

TEST_CASE("Particle bindings - fields write the component, and bad values are script errors")
{
    ParticleWorld world;
    world.WriteScript("Settings.lua", Script("Settings",
        "local function try(label, f) local ok, message = pcall(f); Log.info(label, ok, message) end\n"
        "local particles = self.entity:addParticleSystem()\n"
        "Log.info('same', self.entity:getComponent('ParticleSystemComponent').seed == particles.seed)\n"
        "particles.seed = 42\n"
        "particles.enabled = false\n"
        "particles.playOnStart = false\n"
        "particles.effect = 'Particles\\\\Fire.particle'\n"
        "try('negative', function() particles:emit(-1) end)\n"
        "try('fraction', function() particles:emit(1.5) end)\n"
        "try('nan', function() particles:emit(0/0) end)\n"
        "try('seed', function() particles.seed = -3 end)\n"
        "try('mount', function() particles.effect = 'engine://x.particle' end)\n"
        "try('parent', function() particles.effect = 'Particles/../../x.particle' end)"));
    const ECS::Entity entity = world.AddScripted("Scripts/Settings.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    (void)world.Step();
    CHECK(log.Lines("same true").size() == 1);
    const ParticleSystemComponent& particles = world.Ecs().GetComponent<ParticleSystemComponent>(entity);
    CHECK(particles.Seed == 42u);
    CHECK_FALSE(particles.Enabled);
    CHECK_FALSE(particles.PlayOnStart);
    CHECK(particles.Effect == "Particles/Fire.particle");
    CHECK(log.Lines("negative false").size() == 1);
    CHECK(log.Lines("fraction false").size() == 1);
    CHECK(log.Lines("nan false").size() == 1);
    CHECK(log.Lines("a particle count must be a whole number of at least 0").size() == 3);
    CHECK(log.Lines("seed false").size() == 1);
    CHECK(log.Lines("a seed must be a whole number of at least 0").size() == 1);
    CHECK(log.Lines("mount false").size() == 1);
    CHECK(log.Lines("parent false").size() == 1);
    REQUIRE(world.Stop());
}

TEST_CASE("Particle bindings - outside Play mode play, stop and emit do nothing and the first one logs")
{
    ParticleWorld world;
    const ECS::Entity entity = world.Context.GetSceneManager().CreateGameObject();
    ParticleSystemComponent particles;
    particles.Effect = "Particles/Fire.particle";
    world.Ecs().AddComponent(entity, particles);
    // A script's top level runs when the editor describes it, in Edit mode.
    world.WriteScript("Eager.lua", "local p = Scene.find('" + world.Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name +
                                       "'):getParticleSystem()\np:play()\np:emit(10)\np:stop(true)\n" + Script("Eager", ""));

    LogCapture log;
    (void)world.Scripts->DescribeScript("assets://Scripts/Eager.lua");
    CHECK_FALSE(world.Context.GetParticleSystem()->IsPlaying(world.Ecs(), entity));
    CHECK(world.Ecs().GetComponent<ParticleSystemComponent>(entity).PendingEmit == 0u);
    CHECK(log.Lines("[Script] Particles play only in Play mode; ParticleSystem:play() did nothing.").size() == 1);
    CHECK(log.Lines("Particles play only in Play mode").size() == 1);
    CHECK(world.Step() == 0);
}
