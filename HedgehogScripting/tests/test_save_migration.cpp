#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/Save/SaveGameManager.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include "yaml-cpp/yaml.h"

#include <string>

using HedgehogEngine::LightComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Version 1 of the game keeps a count; version 2 keeps points, ten per count.
    const std::string KEEPER = R"lua(
Keeper = setmetatable({}, { __index = ActorScript })
Keeper.__index = Keeper
function Keeper:new() return setmetatable(ActorScript:new(), Keeper) end
function Keeper:OnStart() self.count = 3 end
function Keeper:OnUpdate(dt) end
function Keeper:OnSave() return { count = self.count } end
function Keeper:OnMigrate(fromVersion, state)
    print("migrating from " .. fromVersion)
    return { points = state.count * 10 }
end
function Keeper:OnLoad(state)
    print("loaded count " .. tostring(state.count) .. " points " .. tostring(state.points))
end
function Keeper:OnDestroy() print("destroyed") end
)lua";

    // Calls fn on every entity node of a World section's tree.
    template<typename Fn>
    void ForEachEntityNode(YAML::Node node, Fn&& fn)
    {
        if (node.IsSequence())
        {
            for (YAML::Node child : node)
                ForEachEntityNode(child, fn);
            return;
        }
        if (!node.IsMap())
            return;
        fn(node);
        if (node["Children"])
            ForEachEntityNode(node["Children"], fn);
    }

    struct MigrationWorld
    {
        TempDir     Saves;
        EngineWorld World;
        ECS::Entity Keeper = 0;
        ECS::Entity Lamp   = 0;

        MigrationWorld()
        {
            World.Context.GetSaveGames().SetSaveDirectory(Saves.Path());
            World.WriteScript("Keeper.lua", KEEPER);
            Keeper = World.AddScripted("Scripts/Keeper.lua");
            Lamp   = World.Context.GetSceneManager().CreateGameObject();
            World.Ecs().GetComponent<ECS::HierarchyComponent>(Lamp).Name = "Lamp";
            World.Ecs().AddComponent(Lamp, LightComponent{});
        }

        HedgehogEngine::SaveGameManager& SaveGames() { return World.Context.GetSaveGames(); }

        // Plays a frame and saves slot at the game data version given.
        void SaveAt(int version, const std::string& slot)
        {
            World.Context.GetSettings().SetGameDataVersion(version);
            REQUIRE(World.Context.Play());
            World.Frame(STEP);
            REQUIRE(SaveGames().RequestSave(slot));
            World.Frame(STEP);
            REQUIRE(World.Stop());
        }

        float Intensity() { return World.Ecs().GetComponent<LightComponent>(Lamp).Intensity; }
    };
}

TEST_CASE("Save migration - a version 1 save loads through a C++ step and the script's OnMigrate")
{
    MigrationWorld world;
    LogCapture     log;
    world.SaveAt(1, "old");
    CHECK(log.Lines("[Save] Saved slot 'old'.").size() == 1);

    // Version 2: the lamps were rebalanced (a C++ step over the World section), and the script
    // migrates its own state.
    world.World.Context.GetSettings().SetGameDataVersion(2);
    world.SaveGames().RegisterMigration(1,
                                        [](EcsSerialization::SaveGameFile& save)
                                        {
                                            ForEachEntityNode(save.Sections["World"]["Scene"],
                                                              [](YAML::Node entity)
                                                              {
                                                                  if (entity["Name"].as<std::string>() == "Lamp")
                                                                      entity["LightComponent"]["LightIntensity"] = 42.0f;
                                                              });
                                            return std::string();
                                        });

    REQUIRE(world.World.Context.Play());
    world.World.Frame(STEP);
    REQUIRE(world.SaveGames().RequestLoad("old"));
    world.World.Frame(STEP); // loads at the end of this frame
    CHECK(log.Lines("[Save] Slot 'old' migrated from game data version 1 to 2.").size() == 1);
    CHECK(world.Intensity() == 42.0f);

    world.World.Frame(STEP); // the restored script: OnMigrate, then OnLoad
    CHECK(log.Lines("migrating from 1").size() == 1);
    CHECK(log.Lines("loaded count nil points 30").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    REQUIRE(world.World.Stop());

    // A save of the current version needs no migration.
    world.SaveAt(2, "current");
    REQUIRE(world.World.Context.Play());
    world.World.Frame(STEP);
    REQUIRE(world.SaveGames().RequestLoad("current"));
    world.World.Frame(STEP);
    world.World.Frame(STEP);
    CHECK(log.Lines("migrating from").size() == 1);
    CHECK(log.Lines("loaded count 3 points nil").size() == 1);
    REQUIRE(world.World.Stop());
}

TEST_CASE("Save migration - a newer save, or a step that fails, is refused and the world is untouched")
{
    MigrationWorld world;
    world.SaveAt(3, "future");
    world.SaveAt(1, "old");
    world.World.Context.GetSettings().SetGameDataVersion(2);
    world.SaveGames().RegisterMigration(1, [](EcsSerialization::SaveGameFile&) { return std::string("the inventory is gone"); });

    LogCapture log;
    REQUIRE(world.World.Context.Play());
    world.World.Frame(STEP);
    world.World.Ecs().GetComponent<LightComponent>(world.Lamp).Intensity = 7.0f;

    CHECK_FALSE(world.SaveGames().RequestLoad("future"));
    CHECK(log.Lines("[Save] Slot 'future' cannot be loaded: the save is game data version 3, but this game reads "
                    "versions 1 to 2.").size() == 1);

    // The step fails at the end of the frame, before the world is replaced.
    REQUIRE(world.SaveGames().RequestLoad("old"));
    world.World.Frame(STEP);
    CHECK(log.Lines("[Save] Slot 'old' cannot be loaded: migrating game data version 1 to 2: the inventory is gone.")
              .size() == 1);
    CHECK(log.Lines("[Save] Loaded slot").empty());
    CHECK(log.Lines("destroyed").empty());
    CHECK(world.Intensity() == 7.0f);
    REQUIRE(world.World.Stop());
}

TEST_CASE("Save migration - Save.list reports each slot's versions and whether it loads")
{
    MigrationWorld world;
    world.SaveAt(3, "future");
    world.SaveAt(1, "old");
    world.World.Context.GetSettings().SetGameDataVersion(2);
    world.World.WriteScript("Lister.lua", R"lua(
Lister = setmetatable({}, { __index = ActorScript })
Lister.__index = Lister
function Lister:new() return setmetatable(ActorScript:new(), Lister) end
function Lister:OnStart()
    for _, slot in ipairs(Save.list()) do
        print("slot " .. slot.name .. " data" .. slot.gameDataVersion .. " v" .. slot.saveVersion .. " loadable " ..
              tostring(slot.loadable) .. " reason " .. tostring(slot.reason))
    end
end
)lua");
    (void)world.World.AddScripted("Scripts/Lister.lua");

    LogCapture log;
    REQUIRE(world.World.Context.Play());
    world.World.Frame(STEP);
    CHECK(log.Lines("slot future data3 v1 loadable false reason the save is game data version 3, but this game reads "
                    "versions 1 to 2").size() == 1);
    CHECK(log.Lines("slot old data1 v1 loadable true reason nil").size() == 1);
    REQUIRE(world.World.Stop());
}

TEST_CASE("Save migration - an OnMigrate that fails faults its script and skips OnLoad")
{
    MigrationWorld world;
    world.World.WriteScript("Keeper.lua", R"lua(
Keeper = setmetatable({}, { __index = ActorScript })
Keeper.__index = Keeper
function Keeper:new() return setmetatable(ActorScript:new(), Keeper) end
function Keeper:OnSave() return { count = 1 } end
function Keeper:OnMigrate(fromVersion, state) error("cannot read version " .. fromVersion) end
function Keeper:OnLoad(state) print("loaded") end
)lua");
    world.SaveAt(1, "old");
    world.World.Context.GetSettings().SetGameDataVersion(2);

    LogCapture log;
    REQUIRE(world.World.Context.Play());
    world.World.Frame(STEP);
    REQUIRE(world.SaveGames().RequestLoad("old"));
    world.World.Frame(STEP);
    world.World.Frame(STEP);
    CHECK(log.Lines("Keeper.lua): OnMigrate failed:").size() == 1);
    CHECK(log.Lines("cannot read version 1").size() >= 1);
    CHECK(log.Lines("loaded").empty());
    REQUIRE(world.World.Stop());
}
