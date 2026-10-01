#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>
#include <vector>

using HedgehogEngine::ScriptComponent;
using HedgehogEngine::ScriptProperty;
using HedgehogEngine::ScriptPropertyType;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A script class named `name`, declaring `properties` (a Lua table literal), whose OnStart
    // runs onStart.
    std::string Script(const std::string& name, const std::string& properties, const std::string& onStart)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name + "\n" +
               "Properties = " + properties + "\n" + "function " + name + ":new() return setmetatable(ActorScript:new(), " +
               name + ") end\n" + "function " + name + ":OnStart()\n" + onStart + "\nend\n";
    }

    // Every declaration helper and both forms, plus a plain global that is not a property.
    const std::string EVERY_TYPE = R"lua({
        speed = 2.5,
        alive = true,
        greeting = "hi",
        offset = Vector3(1, 2, 3),
        tint = Color(0.5, 0.25, 1),
        target = EntityRef(),
        model = AssetRef("Mesh", "Models/a.obj"),
        range = { type = "number", default = 3, min = 0, max = 10, tooltip = "How far" },
    }
notAProperty = 7)lua";

    void PlayOneFrame(EngineWorld& world)
    {
        REQUIRE(world.Context.Play());
        world.Context.UpdatePlayMode(STEP);
        REQUIRE(world.Stop());
    }
}

TEST_CASE("Script properties - DescribeScript lists the declared properties by name with their defaults, and nothing else")
{
    EngineWorld world;
    world.WriteScript("Every.lua", Script("Every", EVERY_TYPE, ""));

    LogCapture log;
    const auto declarations = world.Scripts->DescribeScript("Scripts/Every.lua");
    CHECK(log.Text().empty());

    std::vector<std::string> names;
    for (const auto& declaration : declarations)
        names.push_back(declaration.Default.Name);
    CHECK(names == std::vector<std::string>{ "alive", "greeting", "model", "offset", "range", "speed", "target", "tint" });

    const auto at = [&](size_t index) -> const ScriptProperty& { return declarations[index].Default; };
    CHECK(at(0).Type == ScriptPropertyType::Bool);
    CHECK(std::get<bool>(at(0).Value) == true);
    CHECK(at(1).Type == ScriptPropertyType::String);
    CHECK(std::get<std::string>(at(1).Value) == "hi");
    CHECK(at(2).Type == ScriptPropertyType::AssetRef);
    CHECK(std::get<std::string>(at(2).Value) == "Models/a.obj");
    CHECK(at(2).AssetType == "Mesh");
    CHECK(at(3).Type == ScriptPropertyType::Vector3);
    CHECK(std::get<HM::Vector3>(at(3).Value) == HM::Vector3(1.0f, 2.0f, 3.0f));
    CHECK(at(4).Type == ScriptPropertyType::Number);
    CHECK(std::get<float>(at(4).Value) == 3.0f);
    CHECK(declarations[4].Min == 0.0f);
    CHECK(declarations[4].Max == 10.0f);
    CHECK(declarations[4].Tooltip == "How far");
    CHECK(at(5).Type == ScriptPropertyType::Number);
    CHECK(std::get<float>(at(5).Value) == 2.5f);
    CHECK_FALSE(declarations[5].Min.has_value());
    CHECK(at(6).Type == ScriptPropertyType::EntityRef);
    CHECK(std::get<ECS::Entity>(at(6).Value) == ECS::INVALID_ENTITY);
    CHECK(at(7).Type == ScriptPropertyType::Color);
    CHECK(std::get<HM::Vector3>(at(7).Value) == HM::Vector3(0.5f, 0.25f, 1.0f));
}

TEST_CASE("Script properties - with nothing saved, every declared default is on self before OnStart")
{
    EngineWorld world;
    world.WriteScript("Every.lua", Script("Every", EVERY_TYPE, R"lua(
    print("numbers " .. self.speed .. " " .. self.range .. " alive " .. tostring(self.alive) .. " greeting " .. self.greeting)
    print("offset " .. tostring(self.offset == Vector3(1, 2, 3)) .. " tint " .. tostring(self.tint == Vector3(0.5, 0.25, 1)))
    print("model " .. self.model .. " target valid " .. tostring(self.target:isValid()))
    print("plain global " .. notAProperty .. " not on self " .. tostring(self.notAProperty == nil))
)lua"));
    (void)world.AddScripted("Scripts/Every.lua");

    LogCapture log;
    PlayOneFrame(world);
    CHECK(log.Lines("numbers 2.5 3.0 alive true greeting hi").size() == 1);
    CHECK(log.Lines("offset true tint true").size() == 1);
    CHECK(log.Lines("model Models/a.obj target valid false").size() == 1);
    CHECK(log.Lines("plain global 7 not on self true").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    CHECK(log.Lines("[WARNING]").empty());
}

TEST_CASE("Script properties - saved values of every type reach self, and an EntityRef is a valid handle to the saved entity")
{
    EngineWorld world;
    world.WriteScript("Every.lua", Script("Every", EVERY_TYPE, R"lua(
    print("numbers " .. self.speed .. " alive " .. tostring(self.alive) .. " greeting " .. self.greeting .. " model " .. self.model)
    print("offset " .. tostring(self.offset == Vector3(4, 5, 6)) .. " tint " .. tostring(self.tint == Vector3(0, 1, 0)))
    print("target " .. tostring(self.target:isValid()) .. " " .. self.target.name)
)lua"));
    const ECS::Entity target = world.Context.GetSceneManager().CreateGameObject();
    world.Ecs().GetComponent<ECS::HierarchyComponent>(target).Name = "Target";
    const ECS::Entity entity = world.AddScripted("Scripts/Every.lua");
    world.Ecs().GetComponent<ScriptComponent>(entity).Properties = {
        { "speed", ScriptPropertyType::Number, 9.0f, {} },
        { "alive", ScriptPropertyType::Bool, false, {} },
        { "greeting", ScriptPropertyType::String, std::string("saved"), {} },
        { "model", ScriptPropertyType::AssetRef, std::string("Models/b.obj"), "Mesh" },
        { "offset", ScriptPropertyType::Vector3, HM::Vector3(4.0f, 5.0f, 6.0f), {} },
        { "tint", ScriptPropertyType::Color, HM::Vector3(0.0f, 1.0f, 0.0f), {} },
        { "target", ScriptPropertyType::EntityRef, target, {} },
    };

    LogCapture log;
    PlayOneFrame(world);
    CHECK(log.Lines("numbers 9.0 alive false greeting saved model Models/b.obj").size() == 1);
    CHECK(log.Lines("offset true tint true").size() == 1);
    CHECK(log.Lines("target true Target").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
    CHECK(log.Lines("[WARNING]").empty());
}

TEST_CASE("Script properties - a type mismatch gives the default with one warning; an undeclared saved value is kept with one warning")
{
    EngineWorld world;
    world.WriteScript("Picky.lua", Script("Picky", "{ speed = 2.5 }", "    print(\"speed \" .. self.speed .. \" old \" .. tostring(self.old))"));
    const ECS::Entity entity = world.AddScripted("Scripts/Picky.lua");
    const std::string name   = world.Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name;
    world.Ecs().GetComponent<ScriptComponent>(entity).Properties = {
        { "speed", ScriptPropertyType::Bool, true, {} },
        { "old", ScriptPropertyType::Number, 1.0f, {} },
    };

    LogCapture log;
    PlayOneFrame(world);
    CHECK(log.Lines("speed 2.5 old nil").size() == 1);
    const auto warnings = log.Lines("[WARNING]");
    REQUIRE(warnings.size() == 2);
    const std::string where = "[Script] " + name + " (assets://Scripts/Picky.lua): property '";
    CHECK(warnings[0].find(where + "speed' is saved as bool but declared as number") != std::string::npos);
    CHECK(warnings[1].find(where + "old' is saved but no longer declared") != std::string::npos);
    CHECK(world.Ecs().GetComponent<ScriptComponent>(entity).Properties.size() == 2); // nothing dropped
}

TEST_CASE("Script properties - a declaration that cannot be read is skipped with a warning naming the script")
{
    EngineWorld world;
    world.WriteScript("Broken.lua", Script("Broken", R"lua({
        fine = 1,
        weird = { type = "quaternion" },
        wrong = { type = "number", default = "fast" },
        callback = function() end,
        bare = {},
    })lua", "    print(\"fine \" .. self.fine .. \" weird \" .. tostring(self.weird))"));

    LogCapture log;
    const auto declarations = world.Scripts->DescribeScript("Scripts/Broken.lua");
    REQUIRE(declarations.size() == 1);
    CHECK(declarations[0].Default.Name == "fine");
    const std::string where = "[Script] assets://Scripts/Broken.lua: property '";
    CHECK(log.Lines(where + "weird' is skipped: unknown type 'quaternion'").size() == 1);
    CHECK(log.Lines(where + "wrong' is skipped: its default is not a number").size() == 1);
    CHECK(log.Lines(where + "callback' is skipped").size() == 1);
    CHECK(log.Lines(where + "bare' is skipped: a declaration needs a type or a default").size() == 1);
}
