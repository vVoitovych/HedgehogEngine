#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/PrefabInstanceComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Prefab/PrefabManager.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    std::string Script(const std::string& name, const std::string& top, const std::string& onStart,
                       const std::string& onUpdate = "")
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\nfunction " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" + top + "\n";
        if (!onStart.empty())
            source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onUpdate.empty())
            source += "function " + name + ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
        return source;
    }

    size_t AliveCount(ECS::ECS& ecs)
    {
        size_t count = 0;
        for (ECS::Entity entity = 0; entity < ECS::MAX_ENTITIES; ++entity)
            count += ecs.IsAlive(entity) ? 1 : 0;
        return count;
    }

    // An EngineWorld whose engine reads assets:// from a temp folder too, holding Prefabs/Bullet.prefab:
    // one game object running Scripts/Bullet.lua, which logs its OnStart.
    struct PrefabWorld : EngineWorld
    {
        TempDir Assets;

        PrefabWorld()
        {
            // The engine's mounts, assets:// moved to the temp folder (one file system owns them all).
            FS::FileSystemManager&          files   = Context.GetFileSystem();
            std::unique_ptr<FS::FileSystem> shipped = files.Unregister("assets://");
            REQUIRE(shipped);
            auto replaced = std::make_unique<FS::FileSystem>();
            for (const auto& [alias, path] : shipped->GetMountPoints())
                REQUIRE(replaced->RegisterPath(alias, alias == "assets://" ? Assets.Path() : path));
            REQUIRE(files.Register(std::move(replaced)));

            WriteScript("Bullet.lua", Script("Bullet", "", "print('bullet start ' .. self.entity.name)"));
            const ECS::Entity bullet = Context.GetSceneManager().CreateGameObject();
            Ecs().GetComponent<ECS::HierarchyComponent>(bullet).Name = "Bullet";
            HedgehogEngine::ScriptComponent script;
            script.ScriptPath = "Scripts/Bullet.lua";
            Ecs().AddComponent(bullet, script);
            REQUIRE(Context.GetPrefabs().CreatePrefab(bullet, "assets://Prefabs/Bullet.prefab"));
            Context.GetSceneManager().DeleteGameObject(bullet);
        }
    };
}

TEST_CASE("Prefab bindings - 100 bullets spawned in one frame start on the next, and Stop removes them")
{
    PrefabWorld world;
    // The prefab comes from a declared AssetRef("Prefab") property, as the inspector would wire it.
    world.WriteScript("Gun.lua", Script("Gun", R"lua(Properties = { bullet = AssetRef("Prefab", "Prefabs/Bullet.prefab") })lua", "",
                                        R"lua(
    if fired then return end
    fired = true
    for i = 1, 100 do
        local b = Prefab.instantiate(self.bullet, Vector3(i, 0, 0))
        assert(b:isValid(), "bullet " .. i)
        assert(b.transform.position.x == i)
    end
    print("spawned " .. #Scene.findAll("Bullet"))
)lua"));
    const auto declarations = world.Scripts->DescribeScript("assets://Scripts/Gun.lua");
    REQUIRE(declarations.size() == 1);
    CHECK(declarations[0].Default.AssetType == "Prefab");

    (void)world.AddScripted("Scripts/Gun.lua");
    const size_t atStart = AliveCount(world.Ecs());

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("spawned 100").size() == 1);
    CHECK(log.Lines("bullet start").empty()); // not in the frame that spawned them
    CHECK(AliveCount(world.Ecs()) == atStart + 100);

    world.Frame(STEP);
    CHECK(log.Lines("bullet start Bullet").size() == 100);
    world.Frame(STEP);
    CHECK(log.Lines("bullet start Bullet").size() == 100); // once each

    REQUIRE(world.Stop());
    CHECK(AliveCount(world.Ecs()) == atStart);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Prefab bindings - an instance under a parent, turned, linked to its prefab")
{
    PrefabWorld world;
    world.WriteScript("Spawner.lua", Script("Spawner", "", R"lua(
    local holder = Scene.spawn("Holder")
    local a = Prefab.instantiate("assets://Prefabs/Bullet.prefab", nil, Quat.fromEuler(0, 90, 0), holder)
    local b = Prefab.instantiate("Prefabs\\Bullet.prefab", Vector3(1, 2, 3), Vector3(10, 20, 30))
    assert(a.parent == holder)
    assert(math.abs(a.transform.eulerAngles.y - 90) < 0.001)
    assert(b.transform.eulerAngles == Vector3(10, 20, 30))
    print("instances " .. a.id .. " " .. b.id)
)lua"));
    (void)world.AddScripted("Scripts/Spawner.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    const std::vector<std::string> said = log.Lines("instances ");
    REQUIRE(said.size() == 1);
    // Both instances are one-entity prefab instances, linked as roots.
    size_t linked = 0;
    for (ECS::Entity entity = 0; entity < ECS::MAX_ENTITIES; ++entity)
    {
        if (world.Ecs().IsAlive(entity) && world.Ecs().HasComponent<HedgehogEngine::PrefabInstanceComponent>(entity))
        {
            CHECK(world.Ecs().GetComponent<HedgehogEngine::PrefabInstanceComponent>(entity).PrefabPath ==
                  "assets://Prefabs/Bullet.prefab");
            ++linked;
        }
    }
    CHECK(linked == 2);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Prefab bindings - bad paths and rotations are script errors, a missing prefab an invalid handle")
{
    PrefabWorld world;
    world.WriteScript("Bad.lua", Script("Bad", "", R"lua(
    local missing = Prefab.instantiate("Prefabs/Nothing.prefab")
    print("missing valid " .. tostring(missing:isValid()))
    local ok, err = pcall(Prefab.instantiate, "engine://Prefabs/Bullet.prefab")
    print("mount " .. tostring(ok))
    ok, err = pcall(Prefab.instantiate, "../Bullet.prefab")
    print("parent dir " .. tostring(ok))
    ok, err = pcall(Prefab.instantiate, "Prefabs/Bullet.prefab", nil, 5)
    print("rotation " .. tostring(ok))
)lua"));
    (void)world.AddScripted("Scripts/Bad.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("missing valid false").size() == 1);
    CHECK(log.Lines("[Prefab] assets://Prefabs/Nothing.prefab does not exist.").size() == 1);
    CHECK(log.Lines("mount false").size() == 1);
    CHECK(log.Lines("parent dir false").size() == 1);
    CHECK(log.Lines("rotation false").size() == 1);
}

TEST_CASE("Prefab bindings - outside Play a script's top level spawns nothing")
{
    PrefabWorld world;
    world.WriteScript("Eager.lua", Script("Eager", R"lua(spawned = Prefab.instantiate("Prefabs/Bullet.prefab"))lua", ""));
    const size_t atStart = AliveCount(world.Ecs());

    LogCapture log;
    (void)world.Scripts->DescribeScript("assets://Scripts/Eager.lua");
    (void)world.Scripts->DescribeScript("assets://Scripts/Eager.lua");
    CHECK(AliveCount(world.Ecs()) == atStart);
    CHECK(log.Lines("Prefabs spawn only in Play mode").size() == 1);
}
