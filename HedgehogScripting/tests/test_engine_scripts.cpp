#include "doctest/doctest/doctest.h"

#include "test_log_capture.hpp"

#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ScriptComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "ECS/api/ECS.hpp"

#include <optional>

using HedgehogEngine::EngineContext;
using HedgehogEngine::ScriptComponent;
using HedgehogEngine::TransformComponent;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    // The shipped Default.yaml, loaded through the engine's SceneManager, with the script system
    // registered as the Editor and game mode register it: over the engine's own file system.
    struct ShippedScene
    {
        ShippedScene()
        {
            Scripts = HedgehogScripting::RegisterScriptSystem(Context, Context.GetFileSystem());
            const auto scenePath = Context.GetFileSystem().ResolvePhysical("assets://Scenes/Default.yaml");
            REQUIRE(scenePath.has_value());
            REQUIRE(Context.GetSceneManager().LoadScene(scenePath->string()));
        }

        std::optional<ECS::Entity> FindScripted()
        {
            auto& ecs = Context.GetECS();
            for (ECS::Entity entity = 0; entity < ECS::MAX_ENTITIES; ++entity)
                if (ecs.IsAlive(entity) && ecs.HasComponent<ScriptComponent>(entity))
                    return entity;
            return std::nullopt;
        }

        float RotationZ(ECS::Entity entity) { return Context.GetECS().GetComponent<TransformComponent>(entity).Rotation.z(); }

        void Frames(int count)
        {
            for (int frame = 0; frame < count; ++frame)
                Context.UpdatePlayMode(STEP);
        }

        EngineContext                                    Context;
        std::shared_ptr<HedgehogScripting::ScriptSystem> Scripts;
    };
}

TEST_CASE("Engine scripts - Default.yaml's PlayerScript turns only in Play, and Stop puts it back")
{
    ShippedScene scene;
    const auto   player = scene.FindScripted();
    REQUIRE(player.has_value());
    const float start = scene.RotationZ(*player);

    LogCapture log;
    scene.Frames(60);
    CHECK(scene.RotationZ(*player) == start);

    // speed 7.45, counter-clockwise: one second of play turns it by -7.45 degrees.
    REQUIRE(scene.Context.Play());
    scene.Frames(60);
    CHECK(scene.RotationZ(*player) - start == doctest::Approx(-7.45f).epsilon(0.01f / 7.45f));

    REQUIRE(scene.Context.Pause());
    const float paused = scene.RotationZ(*player);
    scene.Frames(1);
    CHECK(scene.RotationZ(*player) == paused);

    REQUIRE(scene.Context.Stop());
    CHECK(scene.RotationZ(*player) == start);
    CHECK(scene.Scripts->GetScriptCount() == 0);

    CHECK(log.Lines("Base enabled").size() == 1);
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Engine scripts - loading a scene runs no script code and keeps its script path")
{
    LogCapture   log;
    ShippedScene scene;
    const auto   player = scene.FindScripted();
    REQUIRE(player.has_value());

    const auto& script = scene.Context.GetECS().GetComponent<ScriptComponent>(*player);
    CHECK(script.ScriptPath == "Scripts\\PlayerScript.lua");
    const auto* speed = HedgehogEngine::FindScriptProperty(script, "speed");
    REQUIRE(speed != nullptr);
    CHECK(std::get<float>(speed->Value) == doctest::Approx(7.45f));
    CHECK(scene.Scripts->GetScriptCount() == 0);
    CHECK(log.Lines("[Script]").empty());
    CHECK(log.Lines("[ERROR]").empty());
}

TEST_CASE("Engine scripts - DescribeScript on the shipped PlayerScript lists its parameters and prints nothing")
{
    ShippedScene scene;
    LogCapture   log;
    const auto   params = scene.Scripts->DescribeScript("Scripts/PlayerScript.lua");
    REQUIRE(params.size() == 2); // sorted by name
    CHECK(params[0].Name == "clockWise");
    CHECK(params[0].Type == HedgehogEngine::ScriptPropertyType::Bool);
    CHECK(params[1].Name == "speed");
    CHECK(params[1].Type == HedgehogEngine::ScriptPropertyType::Number);
    CHECK(log.Text().empty());
}

TEST_CASE("Engine scripts - a speed edited during Play takes effect at once")
{
    ShippedScene scene;
    const auto   player = scene.FindScripted();
    REQUIRE(player.has_value());

    REQUIRE(scene.Context.Play());
    scene.Frames(1);
    const float before = scene.RotationZ(*player);

    auto& script = scene.Context.GetECS().GetComponent<ScriptComponent>(*player);
    HedgehogEngine::FindScriptProperty(script, "speed")->Value = 60.0f;
    scene.Scripts->PushProperties(scene.Context.GetECS(), *player);
    scene.Frames(1);
    CHECK(scene.RotationZ(*player) - before == doctest::Approx(-1.0f).epsilon(1e-3));
    REQUIRE(scene.Context.Stop());
}
