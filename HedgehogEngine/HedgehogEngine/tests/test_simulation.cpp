#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Simulation/ISimulationSystem.hpp"
#include "HedgehogEngine/api/Simulation/Simulation.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"

#include "ECS/api/ECS.hpp"

#include "FileSystem/api/PathUtils.hpp"

#include <memory>
#include <string>
#include <vector>

using HedgehogEngine::EngineContext;
using HedgehogEngine::ScriptComponent;
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

TEST_CASE("ScriptComponent is data: its path and Params survive Play/Stop and a scene load")
{
    // Nothing in the engine runs scripts; the application's script runtime does, in Play.
    EngineContext context;
    Simulation&   simulation = context.GetSimulation();
    ECS::ECS&     ecs        = context.GetECS();

    const ECS::Entity scripted = context.GetSceneManager().CreateGameObject();
    ScriptComponent   script;
    script.ScriptPath      = "Scripts/PlayerScript.lua";
    script.Params["speed"] = { HedgehogEngine::ParamType::Number, 3.0f, false };
    ecs.AddComponent(scripted, script);

    simulation.Play();
    simulation.Tick(STEP);
    simulation.Stop(); // restores the snapshot through the scene deserializer

    REQUIRE(ecs.HasComponent<ScriptComponent>(scripted));
    const ScriptComponent& restored = ecs.GetComponent<ScriptComponent>(scripted);
    CHECK(restored.ScriptPath == "Scripts/PlayerScript.lua");
    REQUIRE(restored.Params.count("speed") == 1u);
    CHECK(std::get<float>(restored.Params.at("speed").value) == 3.0f);

    const auto scene = FS::GetEngineRootDirectory() / "Assets" / "Scenes" / "Default.yaml";
    REQUIRE(context.GetSceneManager().LoadScene(scene.string()));
    const ECS::Entity player = 1; // Default.yaml's scripted object
    REQUIRE(ecs.HasComponent<ScriptComponent>(player));
    const ScriptComponent& loaded = ecs.GetComponent<ScriptComponent>(player);
    CHECK(loaded.ScriptPath == "Scripts\\PlayerScript.lua");
    CHECK(std::get<float>(loaded.Params.at("speed").value) == doctest::Approx(7.45f));
    CHECK_FALSE(std::get<bool>(loaded.Params.at("clockWise").value));
}
