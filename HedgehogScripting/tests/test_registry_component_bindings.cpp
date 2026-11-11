#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/LightComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Reflection/ComponentMacros.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"

#include "HedgehogMath/api/Vector.hpp"

#include <cstdint>
#include <string>

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A component no hand-written binding knows: one property of each kind.
HH_BEGIN_COMPONENT(TestGadget)
    HH_PROP(float, Speed, 1.0f, None)
    HH_PROP(double, Precise, 0.5, None)
    HH_PROP(int32_t, Count, 3, None)
    HH_PROP(uint32_t, Flags, 0u, None)
    HH_PROP(bool, On, true, None)
    HH_PROP(std::string, Label, std::string("gadget"), None)
    HH_PROP(HM::Vector3, Offset, HM::Vector3(0.0f, 0.0f, 0.0f), None)
    HH_PROP(HM::Vector2, Size, HM::Vector2(2.0f, 4.0f), None)
    HH_PROP(ECS::Entity, Target, ECS::INVALID_ENTITY, EntityRef)
HH_END_COMPONENT(TestGadget)

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

    void RegisterGadget(EngineWorld& world)
    {
        REQUIRE(world.Context.GetComponentTypes().RegisterReflected<TestGadget>(
            EcsSerialization::ComponentDesc{ .Key = "TestGadget", .DisplayName = "Gadget" }));
    }
}

TEST_CASE("Registry component bindings - a script reads and writes a registered component's properties by key")
{
    EngineWorld world;
    RegisterGadget(world);
    world.WriteScript("Tinker.lua", Script("Tinker", R"lua(
    local e = self.entity
    print("before " .. tostring(e:getComponent("TestGadget")) .. " " .. tostring(e:hasComponent("TestGadget")))
    local g = e:addComponent("TestGadget")
    print("defaults " .. g.Speed .. " " .. g.Precise .. " " .. g.Count .. " " .. g.Flags .. " " .. tostring(g.On) ..
          " " .. g.Label .. " " .. g.Size.x .. "x" .. g.Size.y .. " " .. tostring(g.Target))
    g.Speed = 2.5
    g.Precise = 0.125
    g.Count = -7
    g.Flags = 9
    g.On = false
    g.Label = "tuned"
    g.Offset = Vector3(1, 2, 3)
    g.Target = e
    local again = e:addComponent("TestGadget")
    print("again " .. again.Speed .. " " .. tostring(again.Target == e) .. " " .. tostring(e:hasComponent("TestGadget")))
    print("offset " .. tostring(e:getComponent("TestGadget").Offset == Vector3(1, 2, 3)))
)lua"));
    const ECS::Entity tinker = world.AddScripted("Scripts/Tinker.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("before nil false").size() == 1);
    CHECK(log.Lines("defaults 1.0 0.5 3 0 true gadget 2.0x4.0 nil").size() == 1);
    CHECK(log.Lines("again 2.5 true true").size() == 1);
    CHECK(log.Lines("offset true").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());

    REQUIRE(world.Ecs().HasComponent<TestGadget>(tinker));
    const TestGadget& gadget = world.Ecs().GetComponent<TestGadget>(tinker);
    CHECK(gadget.Speed == doctest::Approx(2.5f));
    CHECK(gadget.Precise == doctest::Approx(0.125));
    CHECK(gadget.Count == -7);
    CHECK(gadget.Flags == 9u);
    CHECK_FALSE(gadget.On);
    CHECK(gadget.Label == "tuned");
    CHECK(gadget.Offset.y() == doctest::Approx(2.0f));
    CHECK(gadget.Target == tinker);
}

TEST_CASE("Registry component bindings - wrong values, unknown properties and read-only kinds are script errors")
{
    EngineWorld world;
    RegisterGadget(world);
    const std::string cases[][2] = {
        { "g.Speed = \"fast\"", "TestGadget.Speed takes a number, not string" },
        { "g.Count = 1.5", "TestGadget.Count takes an integer" },
        { "g.Flags = -1", "TestGadget.Flags takes an integer from 0" },
        { "g.On = 1", "TestGadget.On takes a boolean, not number" },
        { "g.Offset = 3", "TestGadget.Offset takes a Vector3" },
        { "g.Size = 3", "TestGadget.Size is read-only for scripts" },
        { "g.Missing = 1", "TestGadget has no property 'Missing'" },
        { "local x = g.Missing", "TestGadget has no property 'Missing'" },
        { "self.entity:getComponent(\"Nope\")", "component type 'Nope' is not registered" },
        { "self.entity:addComponent(\"PrefabInstanceComponent\")", "PrefabInstanceComponent cannot be added by scripts" },
    };
    for (const auto& [statement, message] : cases)
    {
        CAPTURE(statement);
        world.WriteScript("Bad.lua", Script("Bad", "    local g = self.entity:addComponent(\"TestGadget\")\n    " + statement));
        const ECS::Entity bad = world.AddScripted("Scripts/Bad.lua");

        LogCapture log;
        REQUIRE(world.Context.Play());
        world.Frame(STEP);
        REQUIRE(log.Lines("[ERROR]").size() == 1);
        CHECK(log.Lines(message).size() == 1);
        CHECK(log.Lines("[ERROR]")[0].find("Scripts/Bad.lua") != std::string::npos);
        REQUIRE(world.Stop());
        world.Context.GetSceneManager().DeleteGameObject(bad);
    }
}

TEST_CASE("Registry component bindings - engine components with their own bindings are handed out as those")
{
    EngineWorld world;
    world.WriteScript("Lamp.lua", Script("Lamp", R"lua(
    local light = self.entity:addComponent("LightComponent")
    light.intensity = 3
    light.castShadows = true
    print("light " .. tostring(self.entity:getComponent("LightComponent")))
    self.entity:getComponent("TransformComponent").position = Vector3(4, 5, 6)
    print("has " .. tostring(self.entity:hasComponent("LightComponent")))
)lua"));
    const ECS::Entity lamp = world.AddScripted("Scripts/Lamp.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Frame(STEP);
    CHECK(log.Lines("[ERROR]").empty());
    CHECK(log.Lines("light Light of Entity").size() == 1);
    CHECK(log.Lines("has true").size() == 1);
    REQUIRE(world.Ecs().HasComponent<HedgehogEngine::LightComponent>(lamp));
    CHECK(world.Ecs().GetComponent<HedgehogEngine::LightComponent>(lamp).Intensity == doctest::Approx(3.0f));
    CHECK(world.Ecs().GetComponent<HedgehogEngine::LightComponent>(lamp).CastShadows);
    CHECK(world.Ecs().GetComponent<HedgehogEngine::TransformComponent>(lamp).Position.z() == doctest::Approx(6.0f));
}
