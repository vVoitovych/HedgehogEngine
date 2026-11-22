#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/NavMeshComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/NavigationSystem.hpp"
#include "HedgehogEngine/api/Navigation/NavMeshFile.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "HedgehogNavigation/api/NavMeshBake.hpp"
#include "HedgehogNavigation/tests/test_geometry.hpp"

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace
{
    std::string Script(const std::string& name, const std::string& onStart)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
               "\nfunction " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" +
               "function " + name + ":OnStart()\n" + onStart + "\nend\n";
    }

    // Corners as the scripts log them: "x,y,z;" with two decimals.
    std::string Format(const std::vector<HM::Vector3>& corners)
    {
        std::string text;
        for (const HM::Vector3& corner : corners)
        {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.2f,%.2f,%.2f;", corner.x(), corner.y(), corner.z());
            text += buffer;
        }
        return text;
    }

    const std::string FORMAT = "local function format(corners)\n"
                               "  local text = ''\n"
                               "  for _, c in ipairs(corners) do text = text .. string.format('%.2f,%.2f,%.2f;', c.x, c.y, c.z) end\n"
                               "  return text\n"
                               "end\n";

    // Ground from x -15 to 15, y -10 to 10, split along x = 0 by a 3 m wall with a gap from y 2 to
    // 6, and an island beyond x 20 no one can reach, baked into nav://Level.navmesh and named by a
    // NavMeshComponent.
    struct NavigationWorld : EngineWorld
    {
        TempDir NavDir;

        NavigationWorld()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("nav://", NavDir.Path());
            Context.GetFileSystem().Register(std::move(fs));
        }

        void BakeLevel()
        {
            HN::NavMeshGeometry g;
            NavTest::AddFloor(g, -15, -10, 15, 10, 0);
            NavTest::AddBox(g, -0.3f, -10, 0.3f, 2, 3);
            NavTest::AddBox(g, -0.3f, 6, 0.3f, 10, 3);
            NavTest::AddFloor(g, 20, -5, 25, 5, 0);
            const HN::NavMeshBakeResult baked = HN::BakeNavMesh(g, {});
            REQUIRE(baked.Error.empty());
            const std::vector<uint8_t> bytes = HedgehogEngine::WriteNavMeshFile({}, baked.Data);
            NavDir.WriteFile("Level.navmesh", std::string(bytes.begin(), bytes.end()));

            const ECS::Entity             entity = Context.GetSceneManager().CreateGameObject();
            HedgehogEngine::NavMeshComponent component;
            component.NavMesh = "nav://Level.navmesh";
            Ecs().AddComponent(entity, component);
            Context.GetNavigationSystem()->Update(Ecs(), std::chrono::steady_clock::now());
            REQUIRE(Context.GetNavigationSystem()->HasNavMesh());
        }
    };
}

TEST_CASE("Navigation bindings - a script's path goes through the gap, as the engine's does")
{
    NavigationWorld world;
    world.BakeLevel();
    world.WriteScript("Walker.lua", FORMAT + Script("Walker",
        "Log.info('has', Navigation.hasNavMesh())\n"
        "local path = Navigation.findPath(Vector3(-10, -5, 0), Vector3(10, -5, 0))\n"
        "Log.info('status', path.status, #path.corners)\n"
        "Log.info('corners', format(path.corners))\n"
        "local partial = Navigation.findPath(Vector3(-10, 0, 0), Vector3(22, 0, 0))\n"
        "Log.info('island', partial.status)\n"
        "Log.info('nowhere', Navigation.findPath(Vector3(-10, 0, 0), Vector3(0, 0, 50)))\n"
        "local sampled = Navigation.samplePosition(Vector3(5, 5, 0.5))\n"
        "Log.info('sampled', string.format('%.2f', sampled.z))\n"
        "Log.info('far', Navigation.samplePosition(Vector3(5, 5, 20), 1))\n"
        "local hit = Navigation.raycast(Vector3(-5, -5, 0), Vector3(5, -5, 0))\n"
        "Log.info('hit', string.format('%.1f', hit.point.x), string.format('%.0f', hit.normal.x))\n"
        "Log.info('clear', Navigation.raycast(Vector3(-5, -5, 0), Vector3(-5, 5, 0)))"));
    (void)world.AddScripted("Scripts/Walker.lua");

    std::vector<HM::Vector3> corners;
    REQUIRE(world.Context.GetNavigationSystem()->FindPath(HM::Vector3(-10.0f, -5.0f, 0.0f), HM::Vector3(10.0f, -5.0f, 0.0f),
                                                          corners) == HN::NavPathStatus::Complete);
    REQUIRE(corners.size() >= 3);

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(1.0f / 60.0f);
    CHECK(log.Lines("has true").size() == 1);
    CHECK(log.Lines("status complete " + std::to_string(corners.size())).size() == 1);
    CHECK(log.Lines("corners " + Format(corners)).size() == 1);
    CHECK(log.Lines("island partial").size() == 1);
    CHECK(log.Lines("nowhere nil").size() == 1);
    CHECK(log.Lines("sampled 0.").size() == 1);
    CHECK(log.Lines("far nil").size() == 1);
    CHECK(log.Lines("hit -0.").size() == 1);
    CHECK(log.Lines("clear nil").size() == 1);
    CHECK(log.Lines("stack traceback").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Navigation bindings - without a navmesh every query is nil, and in Edit they work")
{
    NavigationWorld world;
    world.WriteScript("Lost.lua", Script("Lost",
        "Log.info('none', Navigation.hasNavMesh(), Navigation.findPath(Vector3(0, 0, 0), Vector3(1, 0, 0)),\n"
        "         Navigation.samplePosition(Vector3(0, 0, 0)), Navigation.raycast(Vector3(0, 0, 0), Vector3(1, 0, 0)))"));
    (void)world.AddScripted("Scripts/Lost.lua");
    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(1.0f / 60.0f);
    CHECK(log.Lines("none false nil nil nil").size() == 1);
    CHECK(log.Lines("stack traceback").empty());
    REQUIRE(world.Stop());

    // A script's top level runs in Edit when the editor describes it; queries only read.
    world.BakeLevel();
    world.WriteScript("Early.lua", "Log.info('edit', Navigation.findPath(Vector3(-10, -5, 0), Vector3(10, -5, 0)).status)\n" +
                                       Script("Early", ""));
    (void)world.Scripts->DescribeScript("assets://Scripts/Early.lua");
    CHECK(log.Lines("edit complete").size() == 1);
}

TEST_CASE("Navigation bindings - bad arguments are script errors")
{
    NavigationWorld world;
    world.BakeLevel();
    world.WriteScript("Bad.lua", Script("Bad",
        "local function try(label, f) local ok, message = pcall(f); Log.info(label, ok, message) end\n"
        "try('nan', function() Navigation.findPath(Vector3(0 / 0, 0, 0), Vector3(1, 0, 0)) end)\n"
        "try('inf', function() Navigation.raycast(Vector3(0, 0, 0), Vector3(1 / 0, 0, 0)) end)\n"
        "try('negative', function() Navigation.samplePosition(Vector3(0, 0, 0), -1) end)\n"
        "try('point', function() Navigation.samplePosition(Vector3(0, 0 / 0, 0)) end)\n"
        "try('type', function() Navigation.findPath('here', Vector3(1, 0, 0)) end)"));
    (void)world.AddScripted("Scripts/Bad.lua");
    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(1.0f / 60.0f);
    for (const char* label : { "nan false", "inf false", "negative false", "point false", "type false" })
    {
        CAPTURE(label);
        CHECK(log.Lines(label).size() == 1);
    }
    CHECK(log.Lines("Navigation: a path's start must be a finite Vector3").size() == 1);
    CHECK(log.Lines("Navigation: a sample distance must be a finite number of at least 0").size() == 1);
    REQUIRE(world.Stop());
}
