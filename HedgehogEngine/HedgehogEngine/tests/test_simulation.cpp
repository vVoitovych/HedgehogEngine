#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Simulation/ISimulationSystem.hpp"
#include "HedgehogEngine/api/Simulation/Simulation.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/ScriptSystem.hpp"

#include "ECS/api/ECS.hpp"

#include <memory>
#include <string>
#include <vector>

using HedgehogEngine::EngineContext;
using HedgehogEngine::ScriptComponent;
using HedgehogEngine::ScriptSystem;
using HedgehogEngine::Simulation;
using HedgehogEngine::SimulationState;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Counts every hook and appends start/stop events to a shared log.
    class RecordingSystem : public HedgehogEngine::ISimulationSystem
    {
    public:
        RecordingSystem(std::string name, std::vector<std::string>& log)
            : m_Name(std::move(name)), m_Log(log)
        {
        }

        void OnPlayStart() override              { m_Log.push_back(m_Name + ".start"); }
        void OnPlayStop() override               { m_Log.push_back(m_Name + ".stop"); }
        void FixedUpdate(float fixedDt) override { ++FixedUpdates; LastFixedDt = fixedDt; }
        void Update(float) override              { ++Updates; }

        int   FixedUpdates = 0;
        int   Updates      = 0;
        float LastFixedDt  = 0.0f;

    private:
        std::string               m_Name;
        std::vector<std::string>& m_Log;
    };

    // A game object running Assets/Scripts/PlayerScript.lua, which spins its Z rotation.
    ECS::Entity CreateScriptedObject(EngineContext& context)
    {
        const ECS::Entity entity = context.GetSceneManager().CreateGameObject();
        ScriptComponent   script;
        script.ScriptPath = "Scripts/PlayerScript.lua";
        context.GetECS().AddComponent(entity, script);
        context.GetScriptSystem()->InitScript(entity, context.GetECS(), context.GetEventBus(),
                                              context.GetFileSystem());
        return entity;
    }

    float RotationZ(EngineContext& context, ECS::Entity entity)
    {
        return context.GetECS().GetComponent<TransformComponent>(entity).Rotation.z();
    }
}

TEST_CASE("Simulation - starts in Edit and ticks nothing there or while paused")
{
    EngineContext context;
    Simulation&   simulation = context.GetSimulation();
    std::vector<std::string> log;
    auto system = std::make_shared<RecordingSystem>("A", log);
    simulation.AddSystem(system);

    CHECK(simulation.GetState() == SimulationState::Edit);
    CHECK(simulation.IsEditing());
    simulation.Tick(STEP);
    CHECK(system->FixedUpdates == 0);
    CHECK(system->Updates == 0);

    simulation.Play();
    simulation.Pause();
    CHECK(simulation.GetState() == SimulationState::Paused);
    simulation.Tick(1.0f);
    CHECK(system->FixedUpdates == 0);
    CHECK(system->Updates == 0);

    simulation.Resume();
    CHECK(simulation.GetState() == SimulationState::Playing);
    simulation.Tick(STEP);
    CHECK(system->FixedUpdates == 1);
    CHECK(system->Updates == 1);

    simulation.Stop();
    CHECK(simulation.GetState() == SimulationState::Edit);
    simulation.Tick(STEP);
    CHECK(system->Updates == 1);
}

TEST_CASE("Simulation - each Tick runs the clock's fixed steps, then one Update")
{
    EngineContext context;
    Simulation&   simulation = context.GetSimulation();
    std::vector<std::string> log;
    auto system = std::make_shared<RecordingSystem>("A", log);
    simulation.AddSystem(system);

    simulation.Play();
    simulation.Tick(1.0f / 30.0f);
    CHECK(system->FixedUpdates == 2);
    CHECK(system->Updates == 1);
    CHECK(system->LastFixedDt == doctest::Approx(simulation.GetClock().GetFixedDeltaTime()));

    simulation.Tick(STEP * 0.5f); // not a whole step yet
    CHECK(system->FixedUpdates == 2);
    CHECK(system->Updates == 2);
    CHECK(simulation.GetInterpolationAlpha() == doctest::Approx(0.5f));

    simulation.Tick(1.0f); // capped at the clock's max substeps
    CHECK(system->FixedUpdates == 2 + static_cast<int>(simulation.GetClock().GetMaxSubsteps()));
    CHECK(system->Updates == 3);
    simulation.Stop();
}

TEST_CASE("Simulation - Play starts systems in order, Stop stops them in reverse")
{
    EngineContext context;
    Simulation&   simulation = context.GetSimulation();
    std::vector<std::string> log;
    simulation.AddSystem(std::make_shared<RecordingSystem>("A", log));
    simulation.AddSystem(std::make_shared<RecordingSystem>("B", log));

    simulation.Play();
    simulation.Play(); // already playing: nothing
    simulation.Pause();
    simulation.Play(); // resumes, does not restart
    simulation.Stop();
    simulation.Stop(); // already stopped: nothing

    const std::vector<std::string> expected{ "A.start", "B.start", "B.stop", "A.stop" };
    CHECK(log == expected);
}

TEST_CASE("Simulation - Stop restores the scene as it was on Play")
{
    EngineContext context;
    auto&         scenes     = context.GetSceneManager();
    Simulation&   simulation = context.GetSimulation();
    ECS::ECS&     ecs        = context.GetECS();

    const ECS::Entity kept    = scenes.CreateGameObject();
    const ECS::Entity doomed  = scenes.CreateGameObject(kept);
    const std::string before  = scenes.CaptureSnapshot().Yaml;

    simulation.Play();
    ecs.GetComponent<TransformComponent>(kept).Position = HM::Vector3(5.0f, 6.0f, 7.0f);
    scenes.DeleteGameObject(doomed);
    const ECS::Entity spawned = scenes.CreateGameObject();
    simulation.Tick(STEP);
    simulation.Stop();

    CHECK(scenes.CaptureSnapshot().Yaml == before);
    CHECK(ecs.IsAlive(kept));
    CHECK(ecs.IsAlive(doomed));
    if (spawned != doomed)
        CHECK_FALSE(ecs.IsAlive(spawned));
}

TEST_CASE("Simulation - a script runs only in Play, freezes on Pause and is undone by Stop")
{
    EngineContext context;
    Simulation&   simulation = context.GetSimulation();
    const ECS::Entity player = CreateScriptedObject(context);
    const float       start  = RotationZ(context, player);

    context.GetSimulation().Tick(0.5f);
    CHECK(RotationZ(context, player) == start); // Edit: no gameplay

    simulation.Play();
    for (int i = 0; i < 10; ++i)
        simulation.Tick(STEP);
    const float played = RotationZ(context, player);
    CHECK(played != start);

    simulation.Pause();
    simulation.Tick(0.5f);
    CHECK(RotationZ(context, player) == played);

    simulation.Stop();
    CHECK(RotationZ(context, player) == start);
}

TEST_CASE("ScriptComponent removal closes the script's lua_State exactly once")
{
    const int baseline = ScriptSystem::GetOpenLuaStateCount();
    {
        EngineContext context;
        const ECS::Entity scripted = CreateScriptedObject(context);
        REQUIRE(ScriptSystem::GetOpenLuaStateCount() == baseline + 1);

        context.GetECS().RemoveComponent<ScriptComponent>(scripted);
        CHECK(ScriptSystem::GetOpenLuaStateCount() == baseline);

        const ECS::Entity deleted = CreateScriptedObject(context);
        REQUIRE(ScriptSystem::GetOpenLuaStateCount() == baseline + 1);
        context.GetSceneManager().DeleteGameObject(deleted);
        CHECK(ScriptSystem::GetOpenLuaStateCount() == baseline);

        (void)CreateScriptedObject(context);
        REQUIRE(ScriptSystem::GetOpenLuaStateCount() == baseline + 1);
    }
    // Destroying the context closes the scripts still open.
    CHECK(ScriptSystem::GetOpenLuaStateCount() == baseline);
}

TEST_CASE("50 Play/Stop cycles leave no lua_State open")
{
    const int baseline = ScriptSystem::GetOpenLuaStateCount();
    {
        EngineContext context;
        Simulation&   simulation = context.GetSimulation();
        (void)CreateScriptedObject(context);
        (void)CreateScriptedObject(context);
        REQUIRE(ScriptSystem::GetOpenLuaStateCount() == baseline + 2);

        for (int cycle = 0; cycle < 50; ++cycle)
        {
            simulation.Play();
            (void)CreateScriptedObject(context); // made during Play, gone after Stop
            simulation.Tick(STEP);
            simulation.Stop();
            REQUIRE(ScriptSystem::GetOpenLuaStateCount() == baseline + 2);
        }

        context.GetSceneManager().ResetScene();
        CHECK(ScriptSystem::GetOpenLuaStateCount() == baseline);
    }
    CHECK(ScriptSystem::GetOpenLuaStateCount() == baseline);
}
