#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/CharacterControllerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>

using HedgehogEngine::CharacterControllerComponent;
using HedgehogEngine::ColliderComponent;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP        = 1.0f / 60.0f;
    constexpr float REST_HEIGHT = 0.5f + 0.9f; // the floor's top and half the default height

    std::string Script(const std::string& name, const std::string& onStart, const std::string& onFixedUpdate = "",
                       const std::string& onUpdate = "")
    {
        std::string source = name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name +
                             "\nfunction " + name + ":new() return setmetatable(ActorScript:new(), " + name +
                             ") end\n";
        if (!onStart.empty())
            source += "function " + name + ":OnStart()\n" + onStart + "\nend\n";
        if (!onFixedUpdate.empty())
            source += "function " + name + ":OnFixedUpdate(dt)\n" + onFixedUpdate + "\nend\n";
        if (!onUpdate.empty())
            source += "function " + name + ":OnUpdate(dt)\n" + onUpdate + "\nend\n";
        return source;
    }

    struct CharacterWorld : EngineWorld
    {
        ECS::Entity FloorEntity = ECS::INVALID_ENTITY;

        CharacterWorld()
        {
            FloorEntity = Context.GetSceneManager().CreateGameObject();
            Ecs().GetComponent<ECS::HierarchyComponent>(FloorEntity).Name = "Floor";
            Ecs().GetComponent<TransformComponent>(FloorEntity).Scale    = HM::Vector3(50.0f, 50.0f, 0.5f);
            Ecs().AddComponent(FloorEntity, ColliderComponent{});
        }

        void Step(int count = 1)
        {
            for (int i = 0; i < count; ++i)
                Context.UpdateContext(1.0f, STEP);
        }

        // A scripted character standing on the floor, with a controller unless the script adds one.
        ECS::Entity Character(const std::string& script, bool withController = true)
        {
            const ECS::Entity entity = AddScripted(script);
            Ecs().GetComponent<TransformComponent>(entity).Position = HM::Vector3(0.0f, 0.0f, REST_HEIGHT + 0.02f);
            Context.GetEventBus().Publish(HedgehogEngine::TransformChangedEvent{ entity });
            if (withController)
                Ecs().AddComponent(entity, CharacterControllerComponent{});
            return entity;
        }

        HM::Vector3 Position(ECS::Entity entity) { return Ecs().GetComponent<TransformComponent>(entity).Position; }
    };
}

TEST_CASE("Character bindings - a script walks its character 1 m in 1 s, jumps and lands, on the floor it names")
{
    CharacterWorld world;
    world.WriteScript("Walker.lua", Script("Walker",
        "self.character = self.entity:getCharacterController()\n"
        "self.steps = 0",
        "self.steps = self.steps + 1\n"
        "if self.steps == 30 then self.character:move(Vector3(1, 0, 0)) end\n"
        "if self.steps == 90 then self.character:move(Vector3(0, 0, 0)) end\n"
        "if self.steps == 100 then self.character:jump(5) end",
        "local c = self.character\n"
        "print('state', tostring(c:isGrounded()), c.groundEntity and c.groundEntity.name or 'none',"
        " string.format('%.3f', c.velocity.x), string.format('%.3f', c.groundNormal.z))"));
    const ECS::Entity character = world.Character("Scripts/Walker.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step(30);
    REQUIRE(world.Context.GetPhysicsSystem()->IsCharacterGrounded(character));
    CHECK(log.Lines("state true Floor").size() >= 1);
    const float start = world.Position(character).x();
    world.Step(60);
    CHECK(world.Position(character).x() - start == doctest::Approx(1.0f).epsilon(0.03));
    CHECK(log.Lines("state true Floor 1.000 1.000").size() >= 1);

    // The jump leaves the ground, and what the script reads matches the engine every frame.
    world.Step(20); // ten steps past the jump on step 100
    CHECK_FALSE(world.Context.GetPhysicsSystem()->IsCharacterGrounded(character));
    CHECK(world.Position(character).z() > REST_HEIGHT + 0.3f);
    CHECK(log.Lines("state false none").size() >= 1);
    world.Step(90);
    CHECK(world.Context.GetPhysicsSystem()->IsCharacterGrounded(character));
    CHECK(world.Position(character).z() == doctest::Approx(REST_HEIGHT).epsilon(0.01));
    CHECK(log.Lines("stack traceback").empty());
    REQUIRE(world.Stop());
}

TEST_CASE("Character bindings - getComponent gives the typed handle; addCharacterController makes one")
{
    CharacterWorld world;
    world.WriteScript("Maker.lua", Script("Maker",
        "print('before', tostring(self.entity:hasCharacterController()))\n"
        "local added = self.entity:addCharacterController()\n"
        "added.stepHeight = 0.2\n"
        "local typed = self.entity:getComponent('CharacterControllerComponent')\n"
        "print('typed', tostring(typed), typed.stepHeight, typed.radius)"));
    const ECS::Entity character = world.Character("Scripts/Maker.lua", false);
    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Step(3);
    CHECK(log.Lines("before false").size() == 1);
    CHECK(log.Lines("typed CharacterController").size() == 1);
    CHECK(log.Lines("0.2").size() >= 1);
    CHECK(world.Context.GetPhysicsSystem()->GetCharacter(character).IsSet());
    CHECK(world.Ecs().GetComponent<CharacterControllerComponent>(character).StepHeight == doctest::Approx(0.2f));
    REQUIRE(world.Stop());
}

TEST_CASE("Character bindings - bad arguments are script errors, and a top-level move in Edit warns once")
{
    const std::pair<std::string, std::string> cases[] = {
        { "c:move(Vector3(0/0, 0, 0))", "a character's velocity must be a finite Vector3" },
        { "c:jump(-1)", "a jump speed must be 0 or more" },
        { "c.layer = 16", "16 is not a physics layer (0 to 15)" },
        { "c.radius = 0", "a radius must be above 0" },
        { "c.height = -2", "a height must be above 0" },
        { "c.maxSlopeAngle = 95", "a slope angle must be between 0 and 90 degrees" },
    };
    for (const auto& [statement, message] : cases)
    {
        CharacterWorld world;
        world.WriteScript("Bad.lua", Script("Bad",
            "local c = self.entity:getCharacterController()\n"
            "local ok, err = pcall(function() " + statement + " end)\n"
            "print('ok', tostring(ok))\nprint(tostring(err))"));
        (void)world.Character("Scripts/Bad.lua");
        LogCapture log;
        REQUIRE(world.Context.Play());
        world.Step(1);
        INFO(statement);
        CHECK(log.Lines("ok false").size() == 1);
        CHECK(log.Lines(message).size() == 1);
        REQUIRE(world.Stop());
    }

    // A script's top level, as the editor describes it, runs in Edit mode.
    CharacterWorld    world;
    const ECS::Entity character = world.Character("Scripts/Move.lua");
    world.Ecs().GetComponent<ECS::HierarchyComponent>(character).Name = "Mover";
    world.WriteScript("Move.lua",
        "local c = Scene.find('Mover'):getCharacterController()\n"
        "c:move(Vector3(1, 0, 0))\nc:move(Vector3(1, 0, 0))\nc:jump(2)\n"
        "print('moved in edit', c.velocity.x)\n");
    LogCapture log;
    (void)world.Scripts->DescribeScript("Scripts/Move.lua");
    CHECK(log.Lines("Physics runs only in Play mode; CharacterController:move() did nothing.").size() == 1);
    CHECK(log.Lines("did nothing").size() == 1);
    CHECK(log.Lines("moved in edit 0.0").size() == 1);
    CHECK(world.Ecs().GetComponent<CharacterControllerComponent>(character).DesiredVelocity.x() == 0.0f);
}

TEST_CASE("Character bindings - a height set during Play rebuilds the character, which then stands taller")
{
    CharacterWorld world;
    world.WriteScript("Grow.lua", Script("Grow", "self.steps = 0",
        "self.steps = self.steps + 1\n"
        "if self.steps == 30 then self.entity:getCharacterController().height = 2.4 end"));
    const ECS::Entity character = world.Character("Scripts/Grow.lua");
    REQUIRE(world.Context.Play());
    world.Step(25);
    CHECK(world.Position(character).z() == doctest::Approx(REST_HEIGHT).epsilon(0.01));
    world.Step(60);
    CHECK(world.Ecs().GetComponent<CharacterControllerComponent>(character).Height == doctest::Approx(2.4f));
    CHECK(world.Position(character).z() == doctest::Approx(0.5f + 1.2f).epsilon(0.01));
    REQUIRE(world.Stop());
    CHECK(world.Ecs().GetComponent<CharacterControllerComponent>(character).Height == doctest::Approx(1.8f));
}
