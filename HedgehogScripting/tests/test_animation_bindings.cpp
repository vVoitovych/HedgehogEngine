#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"

#include <cmath>
#include <string>

using HedgehogEngine::AnimatorComponent;
using HedgehogEngine::MeshComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // The engine's TwoBoneStrip: clips Bend (1 s) and Lift (2 s; Root up 0 -> 1).
    const std::string SKINNED = "Models/Animated/TwoBoneStrip.gltf";

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

    // A scripted entity with the skinned strip loaded into the catalog and the given animator.
    ECS::Entity AddAnimated(EngineWorld& world, const std::string& scriptPath, const AnimatorComponent& animator)
    {
        ECS::ECS&         ecs    = world.Ecs();
        const ECS::Entity entity = world.AddScripted(scriptPath);
        ecs.AddComponent(entity, MeshComponent{});
        world.Context.GetMeshSystem()->LoadMesh(ecs, entity, SKINNED);
        ecs.AddComponent(entity, animator);
        world.Context.GetResourceCatalog().Update(*world.Context.GetRenderSystem(), *world.Context.GetMeshSystem());
        return entity;
    }

    AnimatorComponent Animator(const std::string& clip, bool loop, bool playOnStart)
    {
        AnimatorComponent animator;
        animator.Clip        = clip;
        animator.Loop        = loop;
        animator.PlayOnStart = playOnStart;
        return animator;
    }

    // One frame as EngineContext::UpdateContext runs it: scripts, then animation.
    void Frame(EngineWorld& world, int count = 1)
    {
        for (int i = 0; i < count; ++i)
        {
            world.Context.UpdatePlayMode(STEP);
            world.Context.UpdateAnimation(STEP);
        }
    }

    const AnimatorComponent& State(EngineWorld& world, ECS::Entity entity)
    {
        return world.Ecs().GetComponent<AnimatorComponent>(entity);
    }

    bool IsIdentity(const HM::Matrix4x4& matrix)
    {
        const HM::Matrix4x4 identity = HM::Matrix4x4::GetIdentity();
        for (size_t column = 0; column < 4; ++column)
            for (size_t row = 0; row < 4; ++row)
                if (std::abs(matrix[column][row] - identity[column][row]) > 1e-5f)
                    return false;
        return true;
    }
}

TEST_CASE("Animation bindings - a script plays a clip, crossfades to another and keeps it on an unknown name")
{
    EngineWorld world;
    world.WriteScript("Dancer.lua", Script("Dancer", R"lua(
    frames = 0
    local animator = self.entity:getAnimator()
    print("clips " .. table.concat(animator:getClipNames(), ","))
    print("before " .. tostring(animator.clip) .. " " .. tostring(animator:isPlaying()))
    animator:play("Bend")
)lua", R"lua(
    frames = frames + 1
    local animator = self.entity:getAnimator()
    if frames == 1 then
        print("playing " .. animator.clip .. " " .. tostring(animator:isPlaying()))
    elseif frames == 10 then
        animator:play("Lift", 0.5)
    elseif frames == 11 then
        print("unknown " .. tostring(animator:play("Jump")) .. " " .. animator.clip)
    end
)lua"));
    // Nothing plays on its own: the script starts it.
    const ECS::Entity dancer = AddAnimated(world, "Scripts/Dancer.lua", Animator("", true, false));

    LogCapture log;
    REQUIRE(world.Context.Play());
    Frame(world);
    CHECK(log.Lines("clips Bend,Lift").size() == 1);
    CHECK(log.Lines("before nil false").size() == 1);
    CHECK(log.Lines("playing Bend true").size() == 1);
    CHECK(State(world, dancer).CurrentClip == "Bend");

    Frame(world, 9); // the 10th frame switches to Lift, fading out of Bend over 0.5 s
    CHECK(State(world, dancer).CurrentClip == "Lift");
    CHECK(State(world, dancer).PreviousClip == "Bend");
    CHECK(State(world, dancer).FadeDuration == doctest::Approx(0.5f));
    CHECK(State(world, dancer).CrossfadeTime == doctest::Approx(0.2f)); // the saved field is untouched

    Frame(world); // "Jump" is not a clip of the strip
    CHECK(log.Lines("unknown false Lift").size() == 1);
    CHECK(log.Lines("[WARNING][Animation] Entity " + std::to_string(dancer) + " has no clip 'Jump'").size() == 1);
    CHECK(log.Lines("keeping 'Lift'").size() == 1);
    CHECK(State(world, dancer).CurrentClip == "Lift");
    CHECK(State(world, dancer).PreviousClip == "Bend"); // still fading

    Frame(world, 30); // past the 0.5 s fade
    CHECK(State(world, dancer).PreviousClip.empty());
    CHECK(State(world, dancer).CurrentClip == "Lift");
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Animation bindings - a non-looping clip raises AnimationFinished exactly once per play-through")
{
    EngineWorld world;
    world.WriteScript("Watcher.lua", Script("Watcher", R"lua(
    finishes = 0
    Events.subscribe("AnimationFinished", function(payload)
        finishes = finishes + 1
        print("finished " .. payload.clip .. " " .. tostring(payload.entity == self.entity) .. " " ..
              tostring(self.entity:getAnimator():isPlaying()))
    end)
)lua", R"lua(
    if finishes == 1 and not replayed then
        replayed = true
        self.entity:getAnimator():play("Bend")
    end
)lua"));
    world.WriteScript("Idle.lua", Script("Idle", "", ""));
    AddAnimated(world, "Scripts/Watcher.lua", Animator("Bend", false, true));
    AddAnimated(world, "Scripts/Idle.lua", Animator("Lift", true, true)); // looping: never finishes

    LogCapture log;
    REQUIRE(world.Context.Play());
    Frame(world, 90); // Bend is 1 s long; the rest of the frames hold its end
    CHECK(log.Lines("finished Bend true false").size() == 1);
    CHECK(log.Lines("finished Lift").empty());

    Frame(world, 90); // the script played Bend again once it heard: one more finish, no more
    CHECK(log.Lines("finished Bend true false").size() == 2);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Animation bindings - speed, time, stop, addAnimator and refused arguments")
{
    EngineWorld world;
    world.WriteScript("Driver.lua", Script("Driver", R"lua(
    print("has " .. tostring(self.entity:hasAnimator()) .. " " .. tostring(self.entity:getAnimator()))
    local animator = self.entity:addAnimator()
    print("added " .. tostring(self.entity:hasAnimator()) .. " " .. tostring(animator:isPlaying()))
)lua"));
    world.WriteScript("Strip.lua", Script("Strip", "", R"lua(
    local animator = self.entity:getAnimator()
    if Time.frame == 2 then
        animator.speed = 2
        animator.time = 0.5
        print("set " .. animator.speed .. " " .. animator.time)
    elseif Time.frame == 3 then
        animator:stop()
        print("stopped " .. tostring(animator.clip) .. " " .. tostring(animator:isPlaying()))
    elseif Time.frame == 4 then
        animator:play("Lift", -1)
    end
)lua"));
    const ECS::Entity driver = world.AddScripted("Scripts/Driver.lua");
    const ECS::Entity strip  = AddAnimated(world, "Scripts/Strip.lua", Animator("Lift", true, true));

    LogCapture log;
    REQUIRE(world.Context.Play());
    Frame(world, 2);
    CHECK(log.Lines("has false nil").size() == 1);
    CHECK(log.Lines("added true false").size() == 1);
    CHECK(world.Ecs().HasComponent<AnimatorComponent>(driver));
    CHECK(log.Lines("set 2.0 0.5").size() == 1);
    // Set in OnUpdate, then advanced by the frame at double speed.
    CHECK(State(world, strip).Time == doctest::Approx(0.5f + 2.0f * STEP));

    Frame(world); // stopped: no clip, bind pose
    CHECK(log.Lines("stopped nil false").size() == 1);
    REQUIRE(State(world, strip).Palette.size() == 2u);
    CHECK(IsIdentity(State(world, strip).Palette[0]));
    CHECK(IsIdentity(State(world, strip).Palette[1]));

    Frame(world); // a negative fade is a script error naming the entity's script
    const auto errors = log.Lines("[ERROR]");
    REQUIRE(errors.size() == 1);
    CHECK(errors[0].find("Strip.lua") != std::string::npos);
    CHECK(errors[0].find("crossfade time") != std::string::npos);
    CHECK(State(world, strip).CurrentClip.empty());

    REQUIRE(world.Stop());
    CHECK_FALSE(world.Ecs().HasComponent<AnimatorComponent>(driver)); // added during Play
    CHECK(State(world, strip).Clip == "Lift");                        // the scene as it was
}
