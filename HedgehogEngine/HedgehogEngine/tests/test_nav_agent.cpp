#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/ECS/components/CharacterControllerComponent.hpp"
#include "HedgehogEngine/api/ECS/components/ColliderComponent.hpp"
#include "HedgehogEngine/api/ECS/components/NavAgentComponent.hpp"
#include "HedgehogEngine/api/ECS/components/NavMeshComponent.hpp"
#include "HedgehogEngine/api/ECS/components/TransformComponent.hpp"
#include "HedgehogEngine/api/ECS/systems/NavAgentSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/NavigationSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/NavigationEvents.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"
#include "HedgehogEngine/api/Navigation/NavMeshFile.hpp"
#include "HedgehogEngine/api/Navigation/NavigationGeometry.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "HedgehogNavigation/api/NavMeshBake.hpp"

#include "ECS/api/ECS.hpp"
#include "FileSystem/api/FileSystem.hpp"
#include "FileSystem/tests/test_helpers.hpp"

#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    constexpr float STEP = 1.0f / 60.0f;

    const HM::Vector3 START(-10.0f, -5.0f, 0.0f);
    const HM::Vector3 GOAL(10.0f, -5.0f, 0.0f);

    float FlatDistance(const HM::Vector3& a, const HM::Vector3& b)
    {
        return std::hypot(a.x() - b.x(), a.y() - b.y());
    }

    // Ground from x -15 to 15 and y -10 to 10 (its top at z 0), split along x = 0 by a 3 m wall
    // with a gap from y 2 to 6, all static colliders, baked into nav://Level.navmesh and loaded.
    struct AgentWorld
    {
        TempDir       Dir;
        EngineContext Context;
        ECS::ECS&     Ecs    = Context.GetECS();
        SceneManager& Scenes = Context.GetSceneManager();
        int           Arrivals = 0;

        AgentWorld()
        {
            auto fs = std::make_unique<FS::FileSystem>();
            fs->RegisterPath("nav://", Dir.Path());
            Context.GetFileSystem().Register(std::move(fs));
            Context.GetEventBus().Subscribe<NavAgentArrivedEvent>([this](const NavAgentArrivedEvent&) { ++Arrivals; });

            Box(HM::Vector3(0.0f, 0.0f, -0.5f), HM::Vector3(15.0f, 10.0f, 0.5f));
            Box(HM::Vector3(0.0f, -4.0f, 1.5f), HM::Vector3(0.3f, 6.0f, 1.5f));
            Box(HM::Vector3(0.0f, 8.0f, 1.5f), HM::Vector3(0.3f, 2.0f, 1.5f));
            Context.UpdateContext(1.0f, STEP);

            const HN::NavMeshBuildSettings settings{};
            const HN::NavMeshBakeResult    baked = HN::BakeNavMesh(GatherNavigationGeometry(Ecs, Context.GetResourceCatalog()), settings);
            REQUIRE(baked.Error.empty());
            const std::vector<uint8_t> bytes = WriteNavMeshFile(settings, baked.Data);
            Dir.WriteFile("Level.navmesh", std::string(bytes.begin(), bytes.end()));
            const ECS::Entity navigation = Scenes.CreateGameObject();
            NavMeshComponent  component;
            component.NavMesh = "nav://Level.navmesh";
            Ecs.AddComponent(navigation, component);
            Context.UpdateContext(1.0f, STEP);
            REQUIRE(Context.GetNavigationSystem()->HasNavMesh());
        }

        ECS::Entity Place(const HM::Vector3& position, const HM::Vector3& scale = HM::Vector3(1.0f, 1.0f, 1.0f))
        {
            const ECS::Entity entity    = Scenes.CreateGameObject();
            auto&             transform = Ecs.GetComponent<TransformComponent>(entity);
            transform.Position          = position;
            transform.Scale             = scale;
            Context.GetEventBus().Publish(TransformChangedEvent{ entity });
            return entity;
        }

        void Box(const HM::Vector3& position, const HM::Vector3& halfSize)
        {
            Ecs.AddComponent(Place(position, halfSize), ColliderComponent{}); // the collider's 2 m box times its scale
        }

        ECS::Entity Agent(const HM::Vector3& position)
        {
            const ECS::Entity entity = Place(position);
            Ecs.AddComponent(entity, NavAgentComponent{});
            return entity;
        }

        NavAgentSystem& Agents() { return *Context.GetNavAgentSystem(); }
        HM::Vector3     Position(ECS::Entity entity) { return Ecs.GetComponent<TransformComponent>(entity).Position; }
        NavAgentStatus  Status(ECS::Entity entity) { return Ecs.GetComponent<NavAgentComponent>(entity).Status; }

        // Steps until the agent arrives or steps run out; the largest distance one step moved it.
        float WalkUntilArrived(ECS::Entity entity, int steps)
        {
            float fastest = 0.0f;
            for (int i = 0; i < steps && Status(entity) != NavAgentStatus::Arrived; ++i)
            {
                const HM::Vector3 before = Position(entity);
                Context.UpdateContext(1.0f, STEP);
                fastest = std::max(fastest, FlatDistance(before, Position(entity)));
            }
            return fastest;
        }
    };
}

TEST_CASE("Nav agent - walks around the wall through its gap to its destination, arriving once")
{
    AgentWorld        world;
    const ECS::Entity agent = world.Agent(START);
    REQUIRE(world.Context.Play());
    REQUIRE(world.Agents().SetDestination(world.Ecs, agent, GOAL));
    CHECK(world.Status(agent) == NavAgentStatus::Moving);
    CHECK(world.Agents().GetRemainingDistance(world.Ecs, agent) > FlatDistance(START, GOAL));

    bool        throughGap = false;
    float       fastest    = 0.0f;
    const float speed      = world.Ecs.GetComponent<NavAgentComponent>(agent).Speed;
    for (int i = 0; i < 900 && world.Status(agent) != NavAgentStatus::Arrived; ++i)
    {
        const HM::Vector3 before = world.Position(agent);
        world.Context.UpdateContext(1.0f, STEP);
        const HM::Vector3 after = world.Position(agent);
        fastest                 = std::max(fastest, FlatDistance(before, after));
        if (before.x() < 0.0f && after.x() >= 0.0f)
            throughGap = after.y() > 2.0f && after.y() < 6.0f;
        CHECK(std::abs(after.z()) < 0.3f); // on the ground
    }
    REQUIRE(world.Status(agent) == NavAgentStatus::Arrived);
    CHECK(throughGap);
    CHECK(FlatDistance(world.Position(agent), GOAL) <= world.Ecs.GetComponent<NavAgentComponent>(agent).StoppingDistance + 1e-3f);
    CHECK(fastest <= speed * STEP + 1e-4f);
    CHECK(world.Arrivals == 1);

    for (int i = 0; i < 30; ++i)
        world.Context.UpdateContext(1.0f, STEP);
    CHECK(world.Arrivals == 1);
    CHECK(world.Agents().GetRemainingDistance(world.Ecs, agent) == 0.0f);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Nav agent - with a character controller it walks there under physics")
{
    AgentWorld        world;
    const ECS::Entity agent = world.Agent(START + HM::Vector3(0.0f, 0.0f, 1.0f));
    world.Ecs.AddComponent(agent, CharacterControllerComponent{});
    REQUIRE(world.Context.Play());
    for (int i = 0; i < 30; ++i) // the character is made and lands
        world.Context.UpdateContext(1.0f, STEP);
    REQUIRE(world.Agents().SetDestination(world.Ecs, agent, GOAL));
    (void)world.WalkUntilArrived(agent, 1200);
    CHECK(world.Status(agent) == NavAgentStatus::Arrived);
    CHECK(FlatDistance(world.Position(agent), GOAL) < 0.3f);
    CHECK(world.Arrivals == 1);
    CHECK(world.Ecs.GetComponent<CharacterControllerComponent>(agent).DesiredVelocity.LengthSlow() == 0.0f);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Nav agent - a new destination replans, being pushed off its path replans, and Stop holds it")
{
    AgentWorld        world;
    const ECS::Entity agent = world.Agent(START);
    REQUIRE(world.Context.Play());
    REQUIRE(world.Agents().SetDestination(world.Ecs, agent, GOAL));
    for (int i = 0; i < 60; ++i)
        world.Context.UpdateContext(1.0f, STEP);

    // Somewhere else on its side of the wall.
    const HM::Vector3 other(-10.0f, 8.0f, 0.0f);
    REQUIRE(world.Agents().SetDestination(world.Ecs, agent, other));
    (void)world.WalkUntilArrived(agent, 900);
    REQUIRE(world.Status(agent) == NavAgentStatus::Arrived);
    CHECK(FlatDistance(world.Position(agent), other) < 0.2f);

    // Teleported far off its path, it plans again from where it lands.
    REQUIRE(world.Agents().SetDestination(world.Ecs, agent, GOAL));
    for (int i = 0; i < 20; ++i)
        world.Context.UpdateContext(1.0f, STEP);
    world.Ecs.GetComponent<TransformComponent>(agent).Position = HM::Vector3(-12.0f, -8.0f, 0.0f);
    (void)world.WalkUntilArrived(agent, 1200);
    CHECK(world.Status(agent) == NavAgentStatus::Arrived);
    CHECK(FlatDistance(world.Position(agent), GOAL) < 0.2f);

    // Stop holds it where it is.
    REQUIRE(world.Agents().SetDestination(world.Ecs, agent, START));
    for (int i = 0; i < 30; ++i)
        world.Context.UpdateContext(1.0f, STEP);
    world.Agents().Stop(world.Ecs, agent);
    const HM::Vector3 held = world.Position(agent);
    for (int i = 0; i < 30; ++i)
        world.Context.UpdateContext(1.0f, STEP);
    CHECK(FlatDistance(world.Position(agent), held) == 0.0f);
    CHECK(world.Status(agent) == NavAgentStatus::Idle);

    // A destination off the mesh has no path.
    CHECK_FALSE(world.Agents().SetDestination(world.Ecs, agent, HM::Vector3(0.0f, 0.0f, 50.0f)));
    CHECK(world.Status(agent) == NavAgentStatus::NoPath);
    REQUIRE(world.Context.Stop());
}

TEST_CASE("Nav agent - nothing moves in Edit or Pause, and Stop restores the scene")
{
    AgentWorld        world;
    const ECS::Entity agent = world.Agent(START);
    REQUIRE(world.Agents().SetDestination(world.Ecs, agent, GOAL));
    for (int i = 0; i < 30; ++i)
        world.Context.UpdateContext(1.0f, STEP);
    CHECK(FlatDistance(world.Position(agent), START) == 0.0f);

    const std::string before = world.Scenes.CaptureSnapshot().Yaml;
    REQUIRE(world.Context.Play());
    REQUIRE(world.Agents().SetDestination(world.Ecs, agent, GOAL));
    for (int i = 0; i < 30; ++i)
        world.Context.UpdateContext(1.0f, STEP);
    const HM::Vector3 walked = world.Position(agent);
    CHECK(FlatDistance(walked, START) > 0.5f);

    REQUIRE(world.Context.Pause());
    for (int i = 0; i < 30; ++i)
        world.Context.UpdateContext(1.0f, STEP);
    CHECK(FlatDistance(world.Position(agent), walked) == 0.0f);

    REQUIRE(world.Context.Stop());
    CHECK(FlatDistance(world.Position(agent), START) == 0.0f);
    CHECK(world.Scenes.CaptureSnapshot().Yaml == before);
}
