#include "doctest/doctest/doctest.h"

#include "HedgehogScripting/api/ScriptRuntime.hpp"
#include "HedgehogScripting/api/ScriptVM.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Simulation/Simulation.hpp"

#include "FileSystem/api/PathUtils.hpp"

#include <memory>

using namespace HedgehogEngine;
using HedgehogScripting::ScriptRuntime;
using HedgehogScripting::ScriptVM;

// The engine as the Editor and game mode run it: an EngineContext with the script
// runtime added to its Simulation, over the real Assets folder.
namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // Default.yaml's scripted object: PlayerScript with speed 7.45, clockWise false.
    constexpr ECS::Entity DEFAULT_SCENE_SPINNER = 1;

    struct PlayableEngine
    {
        EngineContext                  Context;
        std::shared_ptr<ScriptRuntime> Runtime;

        PlayableEngine()
        {
            Runtime = std::make_shared<ScriptRuntime>(Context.GetECS(), Context.GetEventBus(), Context.GetFileSystem());
            Context.GetSimulation().AddSystem(Runtime);
        }

        void LoadDefaultScene()
        {
            const auto scene = FS::GetEngineRootDirectory() / "Assets" / "Scenes" / "Default.yaml";
            REQUIRE(Context.GetSceneManager().LoadScene(scene.string()));
            REQUIRE(Context.GetECS().HasComponent<ScriptComponent>(DEFAULT_SCENE_SPINNER));
        }

        float RotationZ(ECS::Entity entity)
        {
            return Context.GetECS().GetComponent<TransformComponent>(entity).Rotation.z();
        }
    };
}

TEST_CASE("Engine scripts - Default.yaml's object turns only in Play, at its saved speed and direction")
{
    PlayableEngine engine;
    engine.LoadDefaultScene();
    Simulation& simulation = engine.Context.GetSimulation();
    const float start      = engine.RotationZ(DEFAULT_SCENE_SPINNER);

    engine.Context.GetSimulation().Tick(STEP);
    CHECK(engine.RotationZ(DEFAULT_SCENE_SPINNER) == start); // Edit
    CHECK(engine.Runtime->GetInstanceCount() == 0u);

    simulation.Play();
    for (int i = 0; i < 10; ++i)
        simulation.Tick(STEP);
    // clockWise false turns it backwards, at the saved 7.45 rather than the script's 1.0.
    const float played = engine.RotationZ(DEFAULT_SCENE_SPINNER);
    CHECK(played == doctest::Approx(start - 10.0f * 7.45f * STEP).epsilon(1e-4));

    simulation.Pause();
    simulation.Tick(STEP);
    CHECK(engine.RotationZ(DEFAULT_SCENE_SPINNER) == played);

    simulation.Stop();
    CHECK(engine.RotationZ(DEFAULT_SCENE_SPINNER) == start);
    CHECK(engine.Runtime->GetInstanceCount() == 0u);

    // The saved parameters survive the Stop restore.
    const ScriptComponent& script = engine.Context.GetECS().GetComponent<ScriptComponent>(DEFAULT_SCENE_SPINNER);
    REQUIRE(script.Params.count("speed") == 1u);
    CHECK(std::get<float>(script.Params.at("speed").value) == doctest::Approx(7.45f));
}

TEST_CASE("Engine scripts - describing a script in Edit lists its parameters and runs nothing")
{
    PlayableEngine engine;
    const auto params = engine.Runtime->DescribeScript("Scripts/PlayerScript.lua");

    CHECK(params.count("speed") == 1u);
    CHECK(params.count("clockWise") == 1u);
    CHECK(engine.Runtime->GetInstanceCount() == 0u);
}

TEST_CASE("Engine scripts - 50 Play/Stop cycles keep one VM and no instances between Plays")
{
    PlayableEngine engine;
    engine.LoadDefaultScene();
    Simulation& simulation = engine.Context.GetSimulation();
    const float start      = engine.RotationZ(DEFAULT_SCENE_SPINNER);

    for (int cycle = 0; cycle < 50; ++cycle)
    {
        simulation.Play();
        simulation.Tick(STEP);
        REQUIRE(engine.Runtime->GetInstanceCount() == 1u);
        simulation.Stop();
        REQUIRE(engine.Runtime->GetInstanceCount() == 0u);
    }
    CHECK(ScriptVM::GetOpenStateCount() == 1);
    CHECK(engine.RotationZ(DEFAULT_SCENE_SPINNER) == start);
}
