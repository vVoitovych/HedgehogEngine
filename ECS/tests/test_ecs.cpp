#include "doctest/doctest/doctest.h"

#include "ECS/api/ECS.hpp"

#include <set>
#include <vector>

namespace
{
    struct Position
    {
        float x{}, y{}, z{};
    };

    struct Velocity
    {
        float dx{}, dy{}, dz{};
    };

    struct PhysicsSystem : ECS::System
    {
    };

    // Fresh ECS with both test components registered.
    ECS::ECS MakeEcs()
    {
        ECS::ECS ecs;
        ecs.Init();
        ecs.RegisterComponent<Position>();
        ecs.RegisterComponent<Velocity>();
        return ecs;
    }
}

// ---------------------------------------------------------------------------
// Entity lifecycle
// ---------------------------------------------------------------------------

TEST_CASE("ECS::CreateEntity - returns distinct in-range ids")
{
    ECS::ECS ecs;
    ecs.Init();

    std::set<ECS::Entity> ids;
    for (int i = 0; i < 100; ++i)
    {
        const ECS::Entity e = ecs.CreateEntity();
        CHECK(e < ECS::MAX_ENTITIES);
        CHECK(ids.insert(e).second); // no duplicates
    }
}

TEST_CASE("ECS::CreateEntity(explicit) - id is removed from the pool")
{
    ECS::ECS ecs;
    ecs.Init();

    const ECS::Entity reserved = 42;
    ecs.CreateEntity(reserved);

    // Exhaust a large part of the pool; the reserved id must never come back.
    for (int i = 0; i < 200; ++i)
        CHECK(ecs.CreateEntity() != reserved);
}

TEST_CASE("ECS::DestroyEntity - id returns to the pool and signature resets")
{
    ECS::ECS ecs = MakeEcs();

    const ECS::Entity e = ecs.CreateEntity();
    ecs.AddComponent(e, Position{ 1.0f, 2.0f, 3.0f });
    REQUIRE(ecs.HasComponent<Position>(e));

    ecs.DestroyEntity(e);
    CHECK_FALSE(ecs.HasComponent<Position>(e));
}

// ---------------------------------------------------------------------------
// Components
// ---------------------------------------------------------------------------

TEST_CASE("ECS components - add/get/has/remove round-trip")
{
    ECS::ECS ecs = MakeEcs();
    const ECS::Entity e = ecs.CreateEntity();

    CHECK_FALSE(ecs.HasComponent<Position>(e));

    ecs.AddComponent(e, Position{ 1.0f, 2.0f, 3.0f });
    REQUIRE(ecs.HasComponent<Position>(e));

    Position& pos = ecs.GetComponent<Position>(e);
    CHECK(pos.x == 1.0f);
    CHECK(pos.y == 2.0f);
    CHECK(pos.z == 3.0f);

    // Mutation through the returned reference must stick.
    pos.x = 10.0f;
    CHECK(ecs.GetComponent<Position>(e).x == 10.0f);

    ecs.RemoveComponent<Position>(e);
    CHECK_FALSE(ecs.HasComponent<Position>(e));
}

TEST_CASE("ECS components - data stays intact after removing other entities")
{
    ECS::ECS ecs = MakeEcs();

    // Packed component arrays use swap-remove internally; deleting every other
    // entity must not corrupt the survivors' data.
    std::vector<ECS::Entity> entities;
    for (int i = 0; i < 100; ++i)
    {
        const ECS::Entity e = ecs.CreateEntity();
        ecs.AddComponent(e, Position{ static_cast<float>(i), 0.0f, 0.0f });
        entities.push_back(e);
    }

    for (size_t i = 0; i < entities.size(); i += 2)
        ecs.DestroyEntity(entities[i]);

    for (size_t i = 1; i < entities.size(); i += 2)
    {
        REQUIRE(ecs.HasComponent<Position>(entities[i]));
        CHECK(ecs.GetComponent<Position>(entities[i]).x == static_cast<float>(i));
    }
}

TEST_CASE("ECS::GetComponentType - distinct types get distinct ids")
{
    ECS::ECS ecs = MakeEcs();
    CHECK(ecs.GetComponentType<Position>() != ecs.GetComponentType<Velocity>());
}

// ---------------------------------------------------------------------------
// Systems
// ---------------------------------------------------------------------------

TEST_CASE("ECS systems - membership follows the entity signature")
{
    ECS::ECS ecs = MakeEcs();

    auto system = ecs.RegisterSystem<PhysicsSystem>();
    REQUIRE(static_cast<bool>(system));
    CHECK(ecs.HasSystem<PhysicsSystem>());

    ECS::Signature signature;
    signature.set(ecs.GetComponentType<Position>());
    signature.set(ecs.GetComponentType<Velocity>());
    ecs.SetSystemSignature<PhysicsSystem>(signature);

    const ECS::Entity e = ecs.CreateEntity();

    ecs.AddComponent(e, Position{});
    CHECK(system->GetEntities().empty()); // only half the signature so far

    ecs.AddComponent(e, Velocity{});
    REQUIRE(system->GetEntities().size() == 1u);
    CHECK(system->GetEntities().front() == e);

    ecs.RemoveComponent<Velocity>(e);
    CHECK(system->GetEntities().empty());
}

TEST_CASE("ECS systems - destroyed entity leaves the system")
{
    ECS::ECS ecs = MakeEcs();

    auto system = ecs.RegisterSystem<PhysicsSystem>();
    ECS::Signature signature;
    signature.set(ecs.GetComponentType<Position>());
    ecs.SetSystemSignature<PhysicsSystem>(signature);

    const ECS::Entity e = ecs.CreateEntity();
    ecs.AddComponent(e, Position{});
    REQUIRE(system->GetEntities().size() == 1u);

    ecs.DestroyEntity(e);
    CHECK(system->GetEntities().empty());
}

// ---------------------------------------------------------------------------
// Root entity
// ---------------------------------------------------------------------------

TEST_CASE("ECS root - starts invalid, tracks SetRoot, resets when destroyed")
{
    ECS::ECS ecs;
    ecs.Init();

    CHECK(ecs.GetRoot() == ECS::INVALID_ENTITY);

    const ECS::Entity root = ecs.CreateEntity();
    ecs.SetRoot(root);
    CHECK(ecs.GetRoot() == root);

    ecs.DestroyEntity(root);
    CHECK(ecs.GetRoot() == ECS::INVALID_ENTITY);
}
