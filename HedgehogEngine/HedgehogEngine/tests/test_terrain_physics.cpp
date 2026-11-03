#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/RigidBodyComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TerrainComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/PhysicsSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"
#include "HedgehogEngine/api/Terrain/TerrainGeometry.hpp"

#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <cmath>
#include <memory>
#include <optional>

using namespace HedgehogEngine;

namespace
{
    constexpr float FRAME = 1.0f / 60.0f;

    // terrains:// on a temp folder holding Flat.terrain (32 m at height 3) and Hills.terrain (32 m of
    // waves), and an engine context to play them in.
    struct TerrainPhysicsScene
    {
        TempDir       Dir;
        EngineContext Context;
        ECS::ECS&     Ecs    = Context.GetECS();
        SceneManager& Scenes = Context.GetSceneManager();
        TerrainData   Hills  = MakeDefaultTerrain();

        TerrainPhysicsScene()
        {
            TerrainData flat     = MakeDefaultTerrain();
            flat.Size            = 32.0f;
            flat.Resolution      = 33;
            flat.SplatResolution = 16;
            flat.Heights.assign(33 * 33, 3.0f);
            flat.Splat.assign(16 * 16 * 4, 0);
            Dir.WriteFile("Flat.terrain", WriteTerrain(flat));

            Hills = flat;
            for (uint32_t row = 0; row < 33; ++row)
                for (uint32_t column = 0; column < 33; ++column)
                    Hills.Heights[row * 33 + column] = 4.0f + 2.0f * std::sin(column * 0.4f) * std::cos(row * 0.3f);
            Dir.WriteFile("Hills.terrain", WriteTerrain(Hills));
            // What the file holds, quantized.
            Hills = *ParseTerrain(WriteTerrain(Hills)).Data;

            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("terrains://", Dir.Path());
            Context.GetFileSystem().Register(std::move(fs));
        }

        ECS::Entity Object(const HM::Vector3& position)
        {
            const ECS::Entity entity = Scenes.CreateGameObject();
            Ecs.GetComponent<TransformComponent>(entity).Position = position;
            Context.GetEventBus().Publish(TransformChangedEvent{ entity });
            return entity;
        }

        ECS::Entity Terrain(const std::string& file, const HM::Vector3& position, bool enabled = true)
        {
            const ECS::Entity entity = Object(position);
            TerrainComponent  terrain;
            terrain.Terrain = "terrains://" + file;
            terrain.Enabled = enabled;
            Ecs.AddComponent(entity, terrain);
            return entity;
        }

        // A dynamic 2 m cube (the default box collider).
        ECS::Entity Cube(const HM::Vector3& position)
        {
            const ECS::Entity cube = Object(position);
            Ecs.AddComponent(cube, ColliderComponent{});
            Ecs.AddComponent(cube, RigidBodyComponent{});
            return cube;
        }

        void Frames(int count)
        {
            for (int frame = 0; frame < count; ++frame)
                Context.UpdateContext(1.0f, FRAME);
        }

        PhysicsSystem& Physics() { return *Context.GetPhysicsSystem(); }
        HM::Vector3    Position(ECS::Entity entity) { return Ecs.GetComponent<TransformComponent>(entity).Position; }
    };
}

TEST_CASE("Terrain physics - a cube dropped on a terrain rests on its surface, moved with its entity")
{
    TerrainPhysicsScene scene;
    // The tile spans world x and y from -16 to 16 and sits 1 m up: its surface is at z = 4.
    const ECS::Entity terrain = scene.Terrain("Flat.terrain", HM::Vector3(-16.0f, -16.0f, 1.0f));
    const ECS::Entity cube    = scene.Cube(HM::Vector3(3.0f, -2.0f, 10.0f));
    scene.Frames(1);

    REQUIRE(scene.Context.Play());
    CHECK(scene.Physics().IsWorldStarted());
    CHECK(scene.Physics().GetBodyCount() == 2);
    CHECK(scene.Physics().GetBody(terrain).IsSet());
    scene.Frames(150);
    // Resting: its 1 m half height on the surface.
    CHECK(scene.Position(cube).z() == doctest::Approx(5.0f).epsilon(0.004));
    CHECK(scene.Position(cube).x() == doctest::Approx(3.0f).epsilon(0.01));

    scene.Context.Stop();
    CHECK(scene.Physics().GetBodyCount() == 0);
    CHECK_FALSE(scene.Physics().GetBody(terrain).IsSet());
}

TEST_CASE("Terrain physics - a ray down hits the terrain entity where its surface is")
{
    TerrainPhysicsScene scene;
    const ECS::Entity   terrain = scene.Terrain("Hills.terrain", HM::Vector3(-16.0f, -16.0f, 0.0f));
    scene.Frames(1);
    REQUIRE(scene.Context.Play());
    scene.Frames(1);

    for (const auto& [x, y] : { std::pair{ 2.3f, 7.9f }, std::pair{ 20.5f, 11.25f }, std::pair{ 30.1f, 0.6f } })
    {
        CAPTURE(x);
        CAPTURE(y);
        const std::optional<PhysicsRayHit> hit = scene.Physics().Raycast(
            HM::Vector3(x - 16.0f, y - 16.0f, 50.0f), HM::Vector3(0.0f, 0.0f, -1.0f), 100.0f);
        REQUIRE(hit);
        CHECK(hit->Entity == terrain);
        // Jolt compresses heights to within 1 mm.
        CHECK(hit->Point.z() == doctest::Approx(SampleTerrainHeight(scene.Hills, x, y)).epsilon(0.002));
        CHECK(hit->Normal.z() > 0.0f);
    }
    // Off the tile nothing is hit.
    CHECK_FALSE(scene.Physics().Raycast(HM::Vector3(20.0f, 0.0f, 50.0f), HM::Vector3(0.0f, 0.0f, -1.0f), 100.0f));
}

TEST_CASE("Terrain physics - only enabled terrains whose file loads make bodies, and a terrain alone starts the world")
{
    TerrainPhysicsScene scene;
    const ECS::Entity   disabled = scene.Terrain("Flat.terrain", HM::Vector3(0.0f, 0.0f, 0.0f), false);
    const ECS::Entity   missing  = scene.Terrain("Missing.terrain", HM::Vector3(0.0f, 0.0f, 0.0f));
    scene.Frames(1);

    // Neither gives the world anything to simulate.
    REQUIRE(scene.Context.Play());
    scene.Frames(2);
    CHECK_FALSE(scene.Physics().IsWorldStarted());
    scene.Context.Stop();

    // An enabled terrain alone starts it; one with a collider of its own is its collider's body.
    scene.Ecs.GetComponent<TerrainComponent>(disabled).Enabled = true;
    const ECS::Entity covered = scene.Terrain("Flat.terrain", HM::Vector3(0.0f, 0.0f, 0.0f));
    scene.Ecs.AddComponent(covered, ColliderComponent{});
    REQUIRE(scene.Context.Play());
    CHECK(scene.Physics().IsWorldStarted());
    CHECK(scene.Physics().GetBody(disabled).IsSet());
    CHECK_FALSE(scene.Physics().GetBody(missing).IsSet());
    CHECK(scene.Physics().GetBodyCount() == 2);

    // Disabled during Play, its body goes at the next step.
    scene.Ecs.GetComponent<TerrainComponent>(disabled).Enabled = false;
    scene.Frames(1);
    CHECK_FALSE(scene.Physics().GetBody(disabled).IsSet());
    CHECK(scene.Physics().GetBodyCount() == 1);
}
