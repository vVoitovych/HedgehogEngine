#include "doctest/doctest/doctest.h"

#include "HedgehogScripting/api/ScriptRuntime.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Simulation/Simulation.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/components/Hierarchy.hpp"

#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/api/FileSystemManager.hpp"
#include "FileSystem/api/PathUtils.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include "test_log_capture.hpp"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace HedgehogEngine;
using HedgehogScripting::ScriptInstance;
using HedgehogScripting::ScriptRuntime;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // A script class named name, derived from ActorScript, with the given methods.
    std::string ScriptClass(const std::string& name, const std::string& methods)
    {
        return name + " = setmetatable({}, { __index = ActorScript })\n" + name + ".__index = " + name + "\n" +
               "function " + name + ":new() return setmetatable(ActorScript:new(), " + name + ") end\n" + methods;
    }

    std::string ShippedBaseScript()
    {
        std::ifstream     file(FS::GetEngineRootDirectory() / "Assets" / "Scripts" / "Base" / "ActorScript.lua");
        std::stringstream text;
        text << file.rdbuf();
        return text.str();
    }

    // The engine's ECS, scenes, Simulation and transform systems, with a script runtime over a
    // temp "assets://" holding the test scripts.
    struct EngineWorld
    {
        TempDir                        Dir;
        FS::FileSystemManager          ScriptFiles;
        EngineContext                  Context;
        std::shared_ptr<ScriptRuntime> Runtime;
        std::vector<std::string>       Log;

        EngineWorld()
        {
            Dir.WriteFile("Scripts/Base/ActorScript.lua", ShippedBaseScript());
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("assets://", Dir.Path());
            REQUIRE(ScriptFiles.Register(std::move(fs)));

            Runtime = std::make_shared<ScriptRuntime>(Context.GetECS(), Context.GetEventBus(), ScriptFiles);
            Context.GetSimulation().AddSystem(Runtime);
            Runtime->GetVM().GetState().set_function("Record", [this](const std::string& event) { Log.push_back(event); });
        }

        ECS::ECS&     Ecs() { return Context.GetECS(); }
        SceneManager& Scenes() { return Context.GetSceneManager(); }
        Simulation&   Sim() { return Context.GetSimulation(); }

        ECS::Entity AddObject(const std::string& name, ECS::Entity parent)
        {
            const ECS::Entity entity = Scenes().CreateGameObject(parent);
            Ecs().GetComponent<ECS::HierarchyComponent>(entity).Name = name;
            return entity;
        }

        ECS::Entity AddScripted(const std::string& name, const std::string& className, const std::string& methods)
        {
            Dir.WriteFile("Scripts/" + className + ".lua", ScriptClass(className, methods));
            const ECS::Entity entity = AddObject(name, Ecs().GetRoot());
            ScriptComponent   script;
            script.ScriptPath = "Scripts/" + className + ".lua";
            Ecs().AddComponent(entity, script);
            return entity;
        }

        // What EngineContext::UpdateContext runs after the Simulation.
        void UpdateTransforms()
        {
            Context.GetTransformSystem()->Update(Ecs(), Context.GetEventBus());
            Context.GetHierarchySystem()->Update(Ecs(), Context.GetEventBus());
        }

        HM::Vector3 WorldPosition(ECS::Entity entity)
        {
            const HM::Vector4& translation = Ecs().GetComponent<TransformComponent>(entity).ObjMatrix[3];
            return HM::Vector3(translation.x(), translation.y(), translation.z());
        }
    };

    bool Near(const HM::Vector3& a, const HM::Vector3& b)
    {
        return (a - b).LengthSqr() < 1e-8f;
    }
}

TEST_CASE("Entity API - setting transform.position moves the entity and its world matrix the same frame")
{
    EngineWorld       world;
    const ECS::Entity mover = world.AddScripted("Mover", "Mover", R"lua(
function Mover:OnUpdate(dt)
    self.entity.transform.position = Vector3(1, 2, 3)
end
)lua");
    world.UpdateTransforms();
    CHECK(Near(world.WorldPosition(mover), HM::Vector3(0.0f, 0.0f, 0.0f)));

    world.Sim().Play();
    world.Sim().Tick(STEP);   // the script sets the position...
    world.UpdateTransforms(); // ...and the same frame's transform pass sees it
    CHECK(Near(world.Ecs().GetComponent<TransformComponent>(mover).Position, HM::Vector3(1.0f, 2.0f, 3.0f)));
    CHECK(Near(world.WorldPosition(mover), HM::Vector3(1.0f, 2.0f, 3.0f)));
    world.Sim().Stop();
}

TEST_CASE("Entity API - self.entity knows its id, name, parent and children")
{
    EngineWorld       world;
    const ECS::Entity parent = world.AddScripted("Tank", "Inspector", R"lua(
function Inspector:OnStart()
    local e = self.entity
    Record(tostring(e:isValid()))
    Record(tostring(e.id))
    Record(e.name)
    Record(tostring(e.parent == nil))
    local kids = e.children
    Record(tostring(#kids))
    Record(kids[1].name)
    Record(tostring(kids[1].parent == e))
    Record(tostring(kids[1] == e))
    Record(tostring(kids[1] == e.children[1]))
    Record(tostring(e))
end
)lua");
    const ECS::Entity turret = world.AddObject("Turret", parent);
    (void)turret;

    world.Sim().Play();
    world.Sim().Tick(STEP);
    // Read before Stop: restoring the scene recreates the entity under a new generation.
    const std::string expectedToString = "Entity " + std::to_string(parent) + " (generation " +
                                         std::to_string(world.Ecs().GetGeneration(parent)) + ")";
    world.Sim().Stop();

    const std::vector<std::string> expected{ "true", std::to_string(parent), "Tank", "true", "1", "Turret", "true",
                                             "false", "true", expectedToString };
    CHECK(world.Log == expected);
}

TEST_CASE("Entity API - transform reads and writes position, rotation, scale and axes")
{
    EngineWorld       world;
    const ECS::Entity entity = world.AddScripted("Box", "Transformer", R"lua(
local function near(a, b) return math.abs(a - b) < 1e-4 end
function Transformer:OnStart()
    local t = self.entity.transform
    t.position = Vector3(1, 2, 3)
    t:translate(Vector3(1, 0, -1))
    assert(t.position == Vector3(2, 2, 2), tostring(t.position))
    t.scale = Vector3(2, 3, 4)
    assert(t.scale == Vector3(2, 3, 4), tostring(t.scale))

    t.eulerAngles = Vector3(0, 90, 0)
    local f, r, u = t.forward, t.right, t.up
    assert(near(f.x, -1) and near(f.y, 0) and near(f.z, 0), "forward " .. tostring(f))
    assert(near(r.x, 0) and near(r.y, 0) and near(r.z, -1), "right " .. tostring(r))
    assert(near(u.x, 0) and near(u.y, 1) and near(u.z, 0), "up " .. tostring(u))

    t.rotation = Quat.axisAngle(Vector3(0, 0, 1), 30)
    local e = t.eulerAngles
    assert(near(e.x, 0) and near(e.y, 0) and near(e.z, 30), "euler " .. tostring(e))
    t:rotate(0, 0, 15)
    assert(near(t.eulerAngles.z, 45), "rotate xyz " .. tostring(t.eulerAngles))
    t:rotate(Quat.axisAngle(Vector3(0, 0, 1), -45))
    assert(near(t.eulerAngles.z, 0), "rotate quat " .. tostring(t.eulerAngles))
    local q = t.rotation
    assert(near(q.w, 1), "rotation " .. tostring(q))
    Record("done")
end
)lua");

    world.Sim().Play();
    world.Sim().Tick(STEP);
    CHECK(world.Log == std::vector<std::string>{ "done" });
    REQUIRE(world.Runtime->FindInstance(entity) != nullptr);
    CHECK(world.Runtime->FindInstance(entity)->GetLastError().empty());

    const TransformComponent& transform = world.Ecs().GetComponent<TransformComponent>(entity);
    CHECK(Near(transform.Position, HM::Vector3(2.0f, 2.0f, 2.0f)));
    CHECK(Near(transform.Scale, HM::Vector3(2.0f, 3.0f, 4.0f)));
    CHECK(transform.Rotation.z() == doctest::Approx(0.0f).epsilon(1e-4));
    world.Sim().Stop();
}

TEST_CASE("Entity API - a handle to a destroyed entity is invalid, even after its id is reused")
{
    EngineWorld       world;
    const ECS::Entity holder = world.AddScripted("Holder", "Keeper", R"lua(
function Keeper:OnStart()
    kept = self.entity.children[1]
end
function Keeper:OnUpdate(dt)
    Record(tostring(kept:isValid()))
    if not kept:isValid() then
        local ok, message = pcall(function() return kept.transform end)
        Record(tostring(ok))
        Record(tostring(message))
        local _ = kept.transform.position -- unprotected: faults this instance
    end
end
)lua");
    const ECS::Entity child = world.AddObject("Kept", holder);

    world.Sim().Play();
    world.Sim().Tick(STEP);
    REQUIRE(world.Log == std::vector<std::string>{ "true" });

    world.Scenes().DeleteGameObject(child);
    // Recycle the child's id: the handle must not start naming the new entity.
    ECS::Entity reused = ECS::INVALID_ENTITY;
    for (size_t attempt = 0; attempt < ECS::MAX_ENTITIES && reused != child; ++attempt)
        reused = world.Scenes().CreateGameObject(holder);
    REQUIRE(reused == child);
    REQUIRE(world.Ecs().IsAlive(child));

    LogCapture capture;
    world.Sim().Tick(STEP);

    REQUIRE(world.Log.size() == 4u);
    CHECK(world.Log[1] == "false");
    CHECK(world.Log[2] == "false");
    CHECK(world.Log[3].find("Entity " + std::to_string(child)) != std::string::npos);
    CHECK(world.Log[3].find("no longer exists") != std::string::npos);

    ScriptInstance* instance = world.Runtime->FindInstance(holder);
    REQUIRE(instance != nullptr);
    CHECK(instance->IsFaulted());
    CHECK(instance->GetLastError().find("Entity " + std::to_string(child)) != std::string::npos);
    CHECK(instance->GetLastError().find("stack traceback") != std::string::npos);
    CHECK(capture.CountLines("[ERROR][Script] Holder (assets://Scripts/Keeper.lua)") == 1);
    world.Sim().Stop();
}

TEST_CASE("Entity API - the legacy functions still work and warn once per script file")
{
    EngineWorld world;
    const std::string methods = R"lua(
function Legacy:OnUpdate(dt)
    local rotation = GetRotation()
    rotation.z = rotation.z + 1
    SetRotation(rotation)
    local position = GetPosition()
    SetPosition({ x = position.x + 2 })
end
)lua";
    const ECS::Entity first  = world.AddScripted("First", "Legacy", methods);
    const ECS::Entity second = world.AddObject("Second", world.Ecs().GetRoot());
    ScriptComponent   script;
    script.ScriptPath = "Scripts/Legacy.lua";
    world.Ecs().AddComponent(second, script);

    LogCapture capture;
    world.Sim().Play();
    for (int i = 0; i < 3; ++i)
        world.Sim().Tick(STEP);

    for (ECS::Entity entity : { first, second })
    {
        const TransformComponent& transform = world.Ecs().GetComponent<TransformComponent>(entity);
        CHECK(transform.Rotation.z() == doctest::Approx(3.0f));
        CHECK(transform.Position.x() == doctest::Approx(6.0f));
    }
    for (const char* function : { "GetRotation", "SetRotation", "GetPosition", "SetPosition" })
    {
        INFO(function);
        CHECK(capture.CountLines(std::string("assets://Scripts/Legacy.lua: ") + function + "() is deprecated") == 1);
    }
    CHECK(capture.CountLines("GetRotation() is deprecated; use self.entity.transform.eulerAngles.") == 1);
    CHECK(capture.CountLines("[WARNING]") == 4);
    world.Sim().Stop();
}
