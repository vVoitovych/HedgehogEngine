#include "doctest/doctest/doctest.h"

#include "test_engine_world.hpp"
#include "test_log_capture.hpp"

#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "ECS/api/components/Hierarchy.hpp"

#include <string>
#include <vector>

using HedgehogEngine::ScriptPropertyType;
using HedgehogEngine::ScriptComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Prints "<event>" for every callback, and "<event> <label>" where it helps to know which
    // entity. `label` is a top-level global, so each entity can be told apart by a parameter.
    const std::string RECORDER = R"lua(
Recorder = setmetatable({}, { __index = ActorScript })
Recorder.__index = Recorder

label = "r"
speed = 1.0
fast = false

function Recorder:new() return setmetatable(ActorScript:new(), Recorder) end
function Recorder:OnStart() print("OnStart " .. label .. " speed " .. speed .. " fast " .. tostring(fast)) end
function Recorder:OnEnable() print("OnEnable " .. label) end
function Recorder:OnDisable() print("OnDisable " .. label) end
function Recorder:OnFixedUpdate(dt) print("OnFixedUpdate " .. label) end
function Recorder:OnUpdate(dt) print("OnUpdate " .. label .. " speed " .. speed) end
function Recorder:OnDestroy() print("OnDestroy " .. label) end
)lua";

    // The script events in the captured log, in order, without print's "[INFO][Script] <file>:<line>: "
    // prefix and trailing space.
    std::vector<std::string> Events(const LogCapture& log)
    {
        std::vector<std::string> events;
        for (std::string line : log.Lines(": On"))
        {
            line = line.substr(line.find(": On") + 2);
            while (!line.empty() && line.back() == ' ')
                line.pop_back();
            events.push_back(line);
        }
        return events;
    }

    void SetNumber(ScriptComponent& component, const std::string& name, float value)
    {
        HedgehogEngine::SetScriptProperty(component, { name, ScriptPropertyType::Number, value, {} });
    }

    void SetBool(ScriptComponent& component, const std::string& name, bool value)
    {
        HedgehogEngine::SetScriptProperty(component, { name, ScriptPropertyType::Bool, value, {} });
    }
}

TEST_CASE("Script lifecycle - a 1/30 s first frame: OnStart, OnEnable, two fixed steps, OnUpdate")
{
    EngineWorld world;
    world.WriteScript("Recorder.lua", RECORDER);
    (void)world.AddScripted("Scripts/Recorder.lua");

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Context.UpdatePlayMode(1.0f / 30.0f);

    const std::vector<std::string> expected{ "OnStart r speed 1.0 fast false", "OnEnable r", "OnFixedUpdate r",
                                             "OnFixedUpdate r", "OnUpdate r speed 1.0" };
    CHECK(Events(log) == expected);
    REQUIRE(world.Stop());
}

TEST_CASE("Script lifecycle - Enable off then on gives OnDisable, then OnEnable, and Stop OnDisable then OnDestroy")
{
    EngineWorld world;
    world.WriteScript("Recorder.lua", RECORDER);
    const ECS::Entity entity = world.AddScripted("Scripts/Recorder.lua");

    REQUIRE(world.Context.Play());
    world.Context.UpdatePlayMode(STEP * 0.5f); // no fixed step yet: only start, enable, update

    LogCapture log;
    world.Ecs().GetComponent<ScriptComponent>(entity).Enable = false;
    world.Context.UpdatePlayMode(STEP * 0.5f);
    world.Context.UpdatePlayMode(STEP * 0.1f);
    world.Ecs().GetComponent<ScriptComponent>(entity).Enable = true;
    world.Context.UpdatePlayMode(STEP * 0.1f);
    REQUIRE(world.Stop());

    // Disabled: no fixed step or update runs. Enabled again: no second OnStart.
    const std::vector<std::string> expected{ "OnDisable r", "OnEnable r", "OnUpdate r speed 1.0", "OnDisable r",
                                             "OnDestroy r" };
    CHECK(Events(log) == expected);
}

TEST_CASE("Script lifecycle - removal, entity destroy and DeleteGameObject each give exactly one OnDestroy")
{
    EngineWorld world;
    world.WriteScript("Recorder.lua", RECORDER);
    const ECS::Entity removed = world.AddScripted("Scripts/Recorder.lua");
    const ECS::Entity deleted = world.AddScripted("Scripts/Recorder.lua");

    // A bare ECS entity, outside the scene tree: destroying a game object with DestroyEntity
    // alone would leave its id in its parent's children, which only SceneManager keeps tidy.
    const ECS::Entity destroyed = world.Ecs().CreateEntity();
    world.Ecs().AddComponent(destroyed, HedgehogEngine::TransformComponent{});
    ScriptComponent destroyedScript;
    destroyedScript.ScriptPath = "Scripts/Recorder.lua";
    world.Ecs().AddComponent(destroyed, destroyedScript);

    REQUIRE(world.Context.Play());
    world.Context.UpdatePlayMode(STEP);

    LogCapture log;
    world.Ecs().RemoveComponent<ScriptComponent>(removed);
    CHECK(log.Lines("OnDestroy").size() == 1);
    world.Ecs().DestroyEntity(destroyed);
    CHECK(log.Lines("OnDestroy").size() == 2);
    world.Context.GetSceneManager().DeleteGameObject(deleted);
    CHECK(log.Lines("OnDestroy").size() == 3);
    CHECK(log.Lines("OnDisable").size() == 3);
    CHECK(world.Scripts->GetScriptCount() == 0);

    world.Context.UpdatePlayMode(STEP);
    REQUIRE(world.Stop());
    CHECK(log.Lines("OnDestroy").size() == 3); // none again at Stop
}

TEST_CASE("Script lifecycle - the engine's own removal callback keeps working during and after Play")
{
    EngineWorld world;
    world.WriteScript("Recorder.lua", RECORDER);
    const ECS::Entity entity = world.AddScripted("Scripts/Recorder.lua");

    int engineCalls = 0;
    world.Ecs().SetComponentRemovedCallback<ScriptComponent>([&engineCalls](ECS::Entity, ScriptComponent&) { ++engineCalls; });

    REQUIRE(world.Context.Play());
    world.Context.UpdatePlayMode(STEP);
    world.Ecs().RemoveComponent<ScriptComponent>(entity);
    CHECK(engineCalls == 1); // chained behind the script system's
    REQUIRE(world.Stop());

    // After Stop the engine's callback is back on its own.
    const ECS::Entity later = world.AddScripted("Scripts/Recorder.lua");
    LogCapture        log;
    world.Ecs().RemoveComponent<ScriptComponent>(later);
    CHECK(engineCalls == 2);
    CHECK(log.Lines("OnDestroy").empty());
}

TEST_CASE("Script lifecycle - a script added during Play starts next frame; one added disabled waits for Enable")
{
    EngineWorld world;
    world.WriteScript("Recorder.lua", RECORDER);

    REQUIRE(world.Context.Play());
    world.Context.UpdatePlayMode(STEP);

    LogCapture log;
    const ECS::Entity late     = world.AddScripted("Scripts/Recorder.lua");
    const ECS::Entity disabled = world.AddScripted("Scripts/Recorder.lua", false);
    CHECK(log.Lines("OnStart").empty());

    world.Context.UpdatePlayMode(STEP);
    CHECK(log.Lines("OnStart").size() == 1);
    CHECK(log.Lines("OnUpdate").size() == 1);

    world.Ecs().GetComponent<ScriptComponent>(disabled).Enable = true;
    world.Context.UpdatePlayMode(STEP);
    CHECK(log.Lines("OnStart").size() == 2);
    CHECK(log.Lines("OnUpdate").size() == 3);
    (void)late;
    REQUIRE(world.Stop());
}

TEST_CASE("Script lifecycle - a faulted script logs once, is skipped, and runs again on the next Play")
{
    EngineWorld world;
    world.WriteScript("Recorder.lua", RECORDER);
    world.WriteScript("Faulty.lua",
                      "Faulty = setmetatable({}, { __index = ActorScript })\n"
                      "Faulty.__index = Faulty\n"
                      "function Faulty:new() return setmetatable(ActorScript:new(), Faulty) end\n"
                      "function Faulty:OnFixedUpdate(dt) error('fixed step failed') end\n"
                      "function Faulty:OnUpdate(dt) print('OnUpdate faulty') end\n"
                      "function Faulty:OnDestroy() print('OnDestroy faulty') end\n");
    (void)world.AddScripted("Scripts/Faulty.lua");
    (void)world.AddScripted("Scripts/Recorder.lua");

    for (int play = 0; play < 2; ++play)
    {
        LogCapture log;
        REQUIRE(world.Context.Play());
        for (int frame = 0; frame < 3; ++frame)
            world.Context.UpdatePlayMode(STEP);
        REQUIRE(world.Stop());

        CHECK(log.Lines("[ERROR][Script]").size() == 1);
        CHECK(log.Text().find("fixed step failed") != std::string::npos);
        CHECK(log.Lines("OnUpdate faulty").empty()); // faulted in its first fixed step
        CHECK(log.Lines("OnUpdate r").size() == 3);  // the other script kept going
        CHECK(log.Lines("OnDestroy faulty").size() == 1);
    }
}

TEST_CASE("Script lifecycle - Properties are the entity's globals at OnStart, and PushProperties updates them")
{
    EngineWorld world;
    world.WriteScript("Recorder.lua", RECORDER);
    const ECS::Entity entity = world.AddScripted("Scripts/Recorder.lua");
    {
        auto& component = world.Ecs().GetComponent<ScriptComponent>(entity);
        SetNumber(component, "speed", 7.5f);
        SetBool(component, "fast", true);
    }

    // Outside Play there is no script to push to.
    world.Scripts->PushProperties(world.Ecs(), entity);

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Context.UpdatePlayMode(STEP * 0.5f);
    CHECK(log.Lines("OnStart r speed 7.5 fast true").size() == 1);
    CHECK(log.Lines("OnUpdate r speed 7.5").size() == 1);

    SetNumber(world.Ecs().GetComponent<ScriptComponent>(entity), "speed", 2.0f);
    world.Context.UpdatePlayMode(STEP * 0.1f);
    CHECK(log.Lines("OnUpdate r speed 7.5").size() == 2); // not pushed yet

    world.Scripts->PushProperties(world.Ecs(), entity);
    world.Context.UpdatePlayMode(STEP * 0.1f);
    CHECK(log.Lines("OnUpdate r speed 2.0").size() == 1);
    REQUIRE(world.Stop());
}

TEST_CASE("Script lifecycle - every property type reaches the script as its Lua value")
{
    EngineWorld world;
    world.WriteScript("Typed.lua", R"lua(
Typed = setmetatable({}, { __index = ActorScript })
Typed.__index = Typed
function Typed:new() return setmetatable(ActorScript:new(), Typed) end
function Typed:OnStart()
    print("offset " .. tostring(offset == Vector3(1, 2, 3)) .. " tint " .. tostring(tint == Vector3(0.5, 0.25, 1)))
    print("greeting " .. greeting .. " model " .. model)
    print("target " .. target.name .. " gone " .. tostring(gone:isValid()))
end
)lua");
    const ECS::Entity target = world.Context.GetSceneManager().CreateGameObject();
    world.Ecs().GetComponent<ECS::HierarchyComponent>(target).Name = "Target";
    const ECS::Entity entity = world.AddScripted("Scripts/Typed.lua");
    {
        auto& component = world.Ecs().GetComponent<ScriptComponent>(entity);
        component.Properties = {
            { "offset", ScriptPropertyType::Vector3, HM::Vector3(1.0f, 2.0f, 3.0f), {} },
            { "tint", ScriptPropertyType::Color, HM::Vector3(0.5f, 0.25f, 1.0f), {} },
            { "greeting", ScriptPropertyType::String, std::string("hi"), {} },
            { "model", ScriptPropertyType::AssetRef, std::string("Models/a.obj"), "Mesh" },
            { "target", ScriptPropertyType::EntityRef, target, {} },
            { "gone", ScriptPropertyType::EntityRef, ECS::Entity(ECS::MAX_ENTITIES - 1), {} },
        };
    }

    LogCapture log;
    REQUIRE(world.Context.Play());
    world.Context.UpdatePlayMode(STEP);
    REQUIRE(world.Stop());
    CHECK(log.Lines("offset true tint true").size() == 1);
    CHECK(log.Lines("greeting hi model Models/a.obj").size() == 1);
    CHECK(log.Lines("target Target gone false").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Script lifecycle - DescribeScript lists the shipped PlayerScript's parameters and logs nothing")
{
    HedgehogEngine::EngineContext context;
    const auto scripts = HedgehogScripting::RegisterScriptSystem(context, context.GetFileSystem());

    LogCapture log;
    const auto params = scripts->DescribeScript("Scripts/PlayerScript.lua");
    CHECK(log.Text().empty());

    REQUIRE(params.size() == 2); // sorted by name
    CHECK(params[0].Name == "clockWise");
    CHECK(params[0].Type == ScriptPropertyType::Bool);
    CHECK(std::get<bool>(params[0].Value) == true);
    CHECK(params[1].Name == "speed");
    CHECK(params[1].Type == ScriptPropertyType::Number);
    CHECK(std::get<float>(params[1].Value) == 1.0f);
    CHECK(scripts->GetScriptCount() == 0);
}

TEST_CASE("Script lifecycle - 50 Play/Stop cycles leave no scripts behind")
{
    EngineWorld world;
    world.WriteScript("Recorder.lua", RECORDER);
    (void)world.AddScripted("Scripts/Recorder.lua");
    (void)world.AddScripted("Scripts/Recorder.lua");

    LogCapture log;
    for (int cycle = 0; cycle < 50; ++cycle)
    {
        REQUIRE(world.Context.Play());
        (void)world.AddScripted("Scripts/Recorder.lua"); // made during Play, gone after Stop
        world.Context.UpdatePlayMode(STEP);
        REQUIRE(world.Stop());
        REQUIRE(world.Scripts->GetScriptCount() == 0);
    }
    CHECK(log.Lines("OnStart").size() == 150);
    CHECK(log.Lines("OnDestroy").size() == 150);
    CHECK(log.Lines("[ERROR][Script]").empty());
}
