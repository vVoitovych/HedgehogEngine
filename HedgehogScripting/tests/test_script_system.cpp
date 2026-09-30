#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>
#include <vector>

using HedgehogScripting::ScriptSystem;

namespace
{
    // Prints every call, so the log shows what ran and in which order.
    const std::string TRACER = R"lua(
Tracer = setmetatable({}, { __index = ActorScript })
Tracer.__index = Tracer

label = "tracer"
count = 0

function Tracer:new()
    local self = setmetatable(ActorScript:new(), Tracer)
    return self
end

function Tracer:OnStart()
    print(label .. " start")
end

function Tracer:OnUpdate(dt)
    count = count + 1
    print(label .. " update " .. dt .. " count " .. count)
end

function Tracer:OnDestroy()
    print(label .. " destroy")
end
)lua";

    std::string NameOf(EngineWorld& world, ECS::Entity entity)
    {
        return world.Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name;
    }
}

TEST_CASE("ScriptSystem - OnStart once, then OnUpdate(dt) per update, OnDestroy at stop")
{
    EngineWorld world;
    world.WriteScript("Tracer.lua", TRACER);
    (void)world.AddScripted("Scripts/Tracer.lua");

    LogCapture log;
    world.Ecs().NotifyPlayStart();
    CHECK(world.Scripts->GetScriptCount() == 1);
    CHECK(log.Lines("[INFO]").empty());

    world.Ecs().RunUpdate(0.25f);
    world.Ecs().RunUpdate(0.5f);
    CHECK(log.Lines("tracer start").size() == 1);
    CHECK(log.Lines("tracer update 0.25 count 1").size() == 1);
    CHECK(log.Lines("tracer update 0.5 count 2").size() == 1);
    CHECK(log.Lines("tracer destroy").empty());

    world.Ecs().NotifyPlayStop();
    CHECK(log.Lines("tracer destroy").size() == 1);
    CHECK(world.Scripts->GetScriptCount() == 0);

    // The start came first and the destroy last.
    const std::string text = log.Text();
    CHECK(text.find("tracer start") < text.find("tracer update"));
    CHECK(text.find("tracer update 0.5") < text.find("tracer destroy"));
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("ScriptSystem - two entities share one compiled class but not their globals")
{
    EngineWorld world;
    world.WriteScript("Tracer.lua", TRACER + R"lua(
function Tracer:OnStart()
    print("class " .. tostring(getmetatable(self)) .. " method " .. tostring(Tracer.OnUpdate))
end
)lua");
    (void)world.AddScripted("Scripts/Tracer.lua");
    (void)world.AddScripted("Scripts/Tracer.lua");

    const int compiledBefore = world.Scripts->GetCompileCount();
    LogCapture log;
    world.Ecs().NotifyPlayStart();
    CHECK(world.Scripts->GetCompileCount() - compiledBefore == 2); // the base, then Tracer.lua once
    CHECK(world.Scripts->GetScriptCount() == 2);

    world.Ecs().RunUpdate(0.25f);
    world.Ecs().RunUpdate(0.25f);

    // Each entity counted its own updates: two lines per count, none reaching 3 or 4.
    CHECK(log.Lines("count 1").size() == 2);
    CHECK(log.Lines("count 2").size() == 2);
    CHECK(log.Lines("count 3").empty());

    // Both instances were made from the same class table and share its methods.
    const auto classLines = log.Lines("class ");
    REQUIRE(classLines.size() == 2);
    CHECK(classLines[0] == classLines[1]);
    world.Ecs().NotifyPlayStop();
}

TEST_CASE("ScriptSystem - through EngineContext, nothing runs in Edit or while paused")
{
    EngineWorld world;
    world.WriteScript("Tracer.lua", TRACER);
    (void)world.AddScripted("Scripts/Tracer.lua");

    LogCapture log;
    world.Context.UpdatePlayMode(1.0f / 60.0f);
    CHECK(log.Lines("tracer").empty());

    REQUIRE(world.Context.Play());
    REQUIRE(world.Context.Pause());
    world.Context.UpdatePlayMode(1.0f / 60.0f);
    CHECK(log.Lines("tracer").empty());

    REQUIRE(world.Context.Resume());
    world.Context.UpdatePlayMode(1.0f / 60.0f);
    CHECK(log.Lines("tracer start").size() == 1);
    CHECK(log.Lines("count 1").size() == 1);

    REQUIRE(world.Context.Stop());
    CHECK(log.Lines("tracer destroy").size() == 1);
    CHECK(world.Scripts->GetScriptCount() == 0);

    world.Context.UpdatePlayMode(1.0f / 60.0f);
    CHECK(log.Lines("count 2").empty());
}

TEST_CASE("ScriptSystem - an OnUpdate error names the entity, the file and line, and the others carry on")
{
    EngineWorld world;
    world.WriteScript("Tracer.lua", TRACER);
    world.WriteScript("Broken.lua",
                      "Broken = setmetatable({}, { __index = ActorScript })\n"
                      "Broken.__index = Broken\n"
                      "function Broken:new() return setmetatable(ActorScript:new(), Broken) end\n"
                      "function Broken:OnUpdate(dt)\n"
                      "    error('broken on purpose')\n"
                      "end\n");
    const ECS::Entity broken = world.AddScripted("Scripts/Broken.lua");
    (void)world.AddScripted("Scripts/Tracer.lua");

    LogCapture log;
    world.Ecs().NotifyPlayStart();
    world.Ecs().RunUpdate(0.25f);
    world.Ecs().RunUpdate(0.25f);

    const auto errors = log.Lines("[ERROR]");
    REQUIRE(errors.size() == 1); // logged once, not every frame
    CHECK(errors[0].find("[Script] " + NameOf(world, broken) + " (assets://Scripts/Broken.lua)") != std::string::npos);
    CHECK(log.Text().find("assets://Scripts/Broken.lua:5:") != std::string::npos);
    CHECK(log.Text().find("broken on purpose") != std::string::npos);
    CHECK(log.Text().find("stack traceback") != std::string::npos);

    // The healthy script kept updating.
    CHECK(log.Lines("count 2").size() == 1);
    world.Ecs().NotifyPlayStop();
}

TEST_CASE("ScriptSystem - a disabled or pathless component gets no script")
{
    EngineWorld world;
    world.WriteScript("Tracer.lua", TRACER);
    (void)world.AddScripted("Scripts/Tracer.lua", false);
    (void)world.AddScripted("");

    world.Ecs().NotifyPlayStart();
    CHECK(world.Scripts->GetScriptCount() == 0);
    world.Ecs().NotifyPlayStop();
}

TEST_CASE("ScriptSystem - script paths normalize to assets:// with forward slashes")
{
    CHECK(ScriptSystem::NormalizeScriptPath("Scripts\\PlayerScript.lua") == "assets://Scripts/PlayerScript.lua");
    CHECK(ScriptSystem::NormalizeScriptPath("Scripts/PlayerScript.lua") == "assets://Scripts/PlayerScript.lua");
    CHECK(ScriptSystem::NormalizeScriptPath("assets://Scripts/PlayerScript.lua") == "assets://Scripts/PlayerScript.lua");
}

TEST_CASE("ScriptSystem - the shipped PlayerScript loads on the shipped ActorScript, by a backslash path")
{
    HedgehogEngine::EngineContext context;
    const auto scripts = HedgehogScripting::RegisterScriptSystem(context, context.GetFileSystem());

    const ECS::Entity player = context.GetSceneManager().CreateGameObject();
    HedgehogEngine::ScriptComponent component;
    component.ScriptPath = "Scripts\\PlayerScript.lua"; // as Default.yaml stores it
    context.GetECS().AddComponent(player, component);

    LogCapture log;
    context.GetECS().NotifyPlayStart();
    CHECK(scripts->GetScriptCount() == 1);
    CHECK(scripts->GetCompileCount() == 2);
    CHECK(log.Lines("[ERROR]").empty());
    context.GetECS().NotifyPlayStop();
}

TEST_CASE("ScriptSystem - a script file is read afresh at each Play")
{
    EngineWorld world;
    world.WriteScript("Tracer.lua", TRACER);
    (void)world.AddScripted("Scripts/Tracer.lua");

    world.Ecs().NotifyPlayStart();
    world.Ecs().NotifyPlayStop();

    world.WriteScript("Tracer.lua", TRACER + "\nlabel = \"edited\"\n");
    LogCapture log;
    world.Ecs().NotifyPlayStart();
    world.Ecs().RunUpdate(0.25f);
    CHECK(log.Lines("edited start").size() == 1);
    world.Ecs().NotifyPlayStop();
}
