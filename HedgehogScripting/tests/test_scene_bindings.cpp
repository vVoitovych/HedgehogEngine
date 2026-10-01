#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A script class named `name` with the given method bodies (empty ones are left out).
    std::string Script(const std::string& name, const std::string& onStart, const std::string& onUpdate = "",
                       const std::string& onDestroy = "")
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\n" + "function " + name + ":new() return setmetatable(ActorScript:new(), " + name +
                             ") end\n";
        if (!onStart.empty())
            source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onUpdate.empty())
            source += "function " + name + ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
        if (!onDestroy.empty())
            source += "function " + name + ":OnDestroy()\n" + onDestroy + "\nend\n";
        return source;
    }

    std::vector<ECS::Entity> AliveEntities(ECS::ECS& ecs)
    {
        std::vector<ECS::Entity> alive;
        for (ECS::Entity entity = 0; entity < ECS::MAX_ENTITIES; ++entity)
            if (ecs.IsAlive(entity))
                alive.push_back(entity);
        return alive;
    }

    // What the test's scripts printed, in order, without the "[INFO][Script] <file>:<line>: "
    // prefix; the base ActorScript's own lines are left out.
    std::vector<std::string> Said(const LogCapture& log)
    {
        std::vector<std::string> said;
        for (const std::string& line : log.Lines("[INFO][Script]"))
        {
            if (line.find("ActorScript.lua") != std::string::npos)
                continue;
            std::string text = line.substr(line.find(": ") + 2);
            text.erase(text.find_last_not_of(' ') + 1);
            said.push_back(text);
        }
        return said;
    }
}

TEST_CASE("Scene bindings - spawning 10 and destroying 5 in one frame leaves 5 more; Stop restores the scene")
{
    EngineWorld world;
    world.WriteScript("Spawner.lua", Script("Spawner", "", R"lua(
    if spawned then return end
    spawned = {}
    for i = 1, 10 do spawned[i] = Scene.spawn("Spawned" .. i) end
    for i = 1, 10, 2 do spawned[i]:destroy() end
    print("valid until the hook ends " .. tostring(spawned[1]:isValid()))
)lua"));
    (void)world.AddScripted("Scripts/Spawner.lua");
    const std::vector<ECS::Entity> atStart = AliveEntities(world.Ecs());

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("valid until the hook ends true").size() == 1);
    CHECK(AliveEntities(world.Ecs()).size() == atStart.size() + 5);
    REQUIRE(world.Stop());
    CHECK(AliveEntities(world.Ecs()) == atStart);
    CHECK(log.Lines("[ERROR]").empty());
    CHECK(log.Lines("[WARNING]").empty());
}

TEST_CASE("Scene bindings - spawn names the object and puts it under a parent, as the editor does")
{
    EngineWorld world;
    world.WriteScript("Maker.lua", Script("Maker", R"lua(
    local box = Scene.spawn("Box", self.entity)
    local plain = Scene.spawn()
    print("box " .. box.name .. " under me " .. tostring(box.parent == self.entity))
    print("plain named " .. tostring(#plain.name > 0) .. " top level " .. tostring(plain.parent == nil))
    print("has transform " .. tostring(box.transform.position == Vector3(0, 0, 0)))
)lua"));
    (void)world.AddScripted("Scripts/Maker.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("box Box under me true").size() == 1);
    CHECK(log.Lines("plain named true top level true").size() == 1);
    CHECK(log.Lines("has transform true").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Scene bindings - a script that destroys itself gets OnDestroy once, after every other OnUpdate")
{
    EngineWorld world;
    world.WriteScript("Doomed.lua", Script("Doomed", "", R"lua(
    print("doomed update")
    self.entity:destroy()
)lua",
                                           "    print(\"doomed destroy\")"));
    world.WriteScript("Watcher.lua", Script("Watcher", "", "    print(\"watcher update\")"));
    const ECS::Entity doomed  = world.AddScripted("Scripts/Doomed.lua");
    const ECS::Entity watcher = world.AddScripted("Scripts/Watcher.lua");
    REQUIRE(doomed < watcher); // scripts run in id order, so the doomed one runs first

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    const std::vector<std::string> expected = { "doomed update", "watcher update", "doomed destroy", "watcher update" };
    CHECK(Said(log) == expected);
    CHECK_FALSE(world.Ecs().IsAlive(doomed));
    CHECK(world.Scripts->GetScriptCount() == 1);
    REQUIRE(world.Stop());
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Scene bindings - destroying twice or through a stale handle does nothing but warn once")
{
    EngineWorld world;
    world.WriteScript("Twice.lua", Script("Twice", R"lua(
    target = Scene.spawn("Target")
    target:destroy()
    Scene.destroy(target)
)lua",
                                          R"lua(
    frames = (frames or 0) + 1
    if frames == 2 then -- the first frame's OnUpdate runs before the queue is flushed
        print("stale " .. tostring(target:isValid()))
        target:destroy()
    end
)lua"));
    (void)world.AddScripted("Scripts/Twice.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP); // OnStart queues it twice: one warning; flushed after the OnUpdates
    CHECK(log.Lines("[WARNING]").size() == 1);
    world.Frame(STEP); // the handle is stale now: one more warning
    CHECK(log.Lines("stale false").size() == 1);
    const auto warnings = log.Lines("[WARNING]");
    REQUIRE(warnings.size() == 2);
    CHECK(warnings[0].find("already being destroyed") != std::string::npos);
    CHECK(warnings[1].find("no longer exists") != std::string::npos);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Scene bindings - find returns the first match or an invalid handle, findAll every match")
{
    EngineWorld world;
    world.WriteScript("Finder.lua", Script("Finder", R"lua(
    local a = Scene.spawn("Crate")
    local holder = Scene.spawn("Holder")
    local b = Scene.spawn("Crate", holder)
    Scene.spawn("Crate")
    local missing = Scene.find("Nothing")
    print("missing valid " .. tostring(missing:isValid()))
    print("first is a " .. tostring(Scene.find("Crate") == a))
    local all = Scene.findAll("Crate")
    print("all " .. #all .. " " .. tostring(all[1] == a) .. " " .. tostring(all[2] == b))
    print("none " .. #Scene.findAll("Nothing"))
)lua"));
    (void)world.AddScripted("Scripts/Finder.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("missing valid false").size() == 1);
    CHECK(log.Lines("first is a true").size() == 1);
    // Hierarchy order, depth first: the crate under Holder comes before the top-level one spawned after Holder.
    CHECK(log.Lines("all 3 true true").size() == 1);
    CHECK(log.Lines("none 0").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Scene bindings - destroying a parent moves its children up to the grandparent")
{
    EngineWorld world;
    world.WriteScript("Family.lua", Script("Family", R"lua(
    parent = Scene.spawn("Parent", self.entity)
    child = Scene.spawn("Child", parent)
    parent:destroy()
)lua",
                                           R"lua(
    if checked then return end
    checked = true
    print("child valid " .. tostring(child:isValid()) .. ", parent valid " .. tostring(parent:isValid()))
    print("child under me " .. tostring(child.parent == self.entity))
)lua"));
    const ECS::Entity grandparent = world.AddScripted("Scripts/Family.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    world.Frame(STEP);
    CHECK(log.Lines("child valid true, parent valid false").size() == 1);
    CHECK(log.Lines("child under me true").size() == 1);
    const auto& children = world.Ecs().GetComponent<ECS::HierarchyComponent>(grandparent).Children;
    CHECK(children.size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.Stop());
}
