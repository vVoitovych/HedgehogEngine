#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/AnimatorComponent.hpp"
#include "HedgehogEngine/api/ECS/components/MeshComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/AnimationSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/MeshSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/RenderSystem.hpp"

#include <string>

using HedgehogEngine::AnimatorComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // FeatureTest's sample (the engine's own assets://): Bending and Lifting on the Lifted bool,
    // and the Wave trigger starting Bending again from any state.
    const std::string CONTROLLER = "Animations/TwoBoneStrip.animctrl";

    std::string Script(const std::string& name, const std::string& onStart, const std::string& onUpdate = "")
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\nfunction " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n";
        if (!onStart.empty())
            source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onUpdate.empty())
            source += "function " + name + ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
        return source;
    }

    // A scripted entity holding the skinned strip and an animator naming controller.
    ECS::Entity AddStrip(EngineWorld& world, const std::string& scriptPath, const std::string& controller)
    {
        ECS::ECS&         ecs    = world.Ecs();
        const ECS::Entity entity = world.AddScripted(scriptPath);
        ecs.AddComponent(entity, HedgehogEngine::MeshComponent{});
        world.Context.GetMeshSystem()->LoadMesh(ecs, entity, "Models/Animated/TwoBoneStrip.gltf");
        AnimatorComponent animator;
        animator.Clip       = "Bend";
        animator.Controller = controller;
        ecs.AddComponent(entity, animator);
        world.Context.GetResourceCatalog().Update(*world.Context.GetRenderSystem(), *world.Context.GetMeshSystem());
        return entity;
    }

    // Scripts, then animation, as UpdateContext runs them.
    void Frame(EngineWorld& world, int count = 1)
    {
        for (int i = 0; i < count; ++i)
        {
            world.Context.UpdatePlayMode(STEP);
            world.Context.UpdateAnimation(STEP);
        }
    }
}

TEST_CASE("Animator controller bindings - a script sets parameters, reads the state and hears each change")
{
    EngineWorld world;
    world.WriteScript("Driver.lua", Script("Driver", R"lua(
    local animator = self.entity:getAnimator()
    print("controller " .. animator.controller .. " state " .. tostring(animator.state))
    animator:setBool("Lifted", true)
    print("lifted " .. tostring(animator:getParameter("Lifted")) .. " wave " .. tostring(animator:getParameter("Wave")))
    Events.subscribe("AnimatorStateChanged", function(event)
        if event.entity == self.entity then
            print("changed " .. tostring(event.from) .. " " .. event.to)
        end
    end)
    self.frames = 0
)lua", R"lua(
    self.frames = self.frames + 1
    local animator = self.entity:getAnimator()
    if self.frames == 3 then
        print("state " .. animator.state)
        animator:setTrigger("Wave")
    elseif self.frames == 5 then
        print("state " .. animator.state)
        print("play " .. tostring(animator:play("Lifting", 0)))
    elseif self.frames == 7 then
        print("state " .. animator.state)
    end
)lua"));
    const ECS::Entity strip = AddStrip(world, "Scripts/Driver.lua", CONTROLLER);

    LogCapture log;
    REQUIRE(world.Context.Play());
    Frame(world, 8);
    CHECK(log.Lines("controller " + CONTROLLER + " state nil").size() == 1);
    CHECK(log.Lines("lifted true wave false").size() == 1);
    // Frame 1 enters the default state; Lifted, set before it, moves it on in frame 2.
    CHECK(log.Lines("changed nil Bending").size() == 1);
    CHECK(log.Lines("state Lifting").size() == 3); // frames 3, 5 and 7
    // The trigger set in frame 3 starts Bending from any state, once; Lifted, still set, moves
    // it on to Lifting again the next frame.
    CHECK(log.Lines("changed Lifting Bending").size() == 1);
    CHECK(log.Lines("changed Bending Lifting").size() == 2);
    // play names a state: Lifting entered again from its start.
    CHECK(log.Lines("play true").size() == 1);
    CHECK(log.Lines("changed Lifting Lifting").size() == 1);
    CHECK(world.Context.GetAnimationSystem()->GetStateName(world.Ecs(), strip) == "Lifting");
    CHECK(world.Context.GetAnimationSystem()->GetParameter(world.Ecs(), strip, "Wave") == 0.0f);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Animator controller bindings - a parameter of another name or type, or no controller, is a script error")
{
    EngineWorld world;
    world.WriteScript("Strict.lua", Script("Strict", R"lua(
    local animator = self.entity:getAnimator()
    local function try(label, fn)
        local ok, message = pcall(fn)
        print(label .. " " .. tostring(ok) .. " " .. tostring(message))
    end
    try("float", function() animator:setFloat("Lifted", 1) end)
    try("unknown", function() animator:setBool("Running", true) end)
    try("get", function() return animator:getParameter("Running") end)
    try("trigger", function() animator:setTrigger("Lifted") end)
    try("int", function() animator:setInt("Lifted", 1.5) end)
)lua"));
    world.WriteScript("Plain.lua", Script("Plain", R"lua(
    local animator = self.entity:getAnimator()
    local ok, message = pcall(function() animator:setBool("Lifted", true) end)
    print("plain " .. tostring(ok) .. " " .. tostring(message) .. " state " .. tostring(animator.state) ..
          " controller " .. tostring(animator.controller))
)lua"));
    (void)AddStrip(world, "Scripts/Strict.lua", CONTROLLER);
    (void)AddStrip(world, "Scripts/Plain.lua", "");

    LogCapture log;
    REQUIRE(world.Context.Play());
    Frame(world, 2);
    CHECK(log.Lines("float false").size() == 1);
    CHECK(log.Lines("has no Float parameter 'Lifted'").size() == 1);
    CHECK(log.Lines("has no Bool parameter 'Running'").size() == 1);
    CHECK(log.Lines("has no parameter 'Running'").size() == 1);
    CHECK(log.Lines("has no Trigger parameter 'Lifted'").size() == 1);
    CHECK(log.Lines("has no Int parameter 'Lifted'").size() == 1);
    CHECK(log.Lines("plain false").size() == 1);
    CHECK(log.Lines("the animator has no controller").size() == 1);
    CHECK(log.Lines("state nil controller nil").size() == 1);
    REQUIRE(world.Stop());
}
