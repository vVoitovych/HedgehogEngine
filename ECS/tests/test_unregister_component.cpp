#include "doctest/doctest/doctest.h"

#include "ECS/api/ECS.hpp"

#include <algorithm>
#include <vector>

namespace
{
    struct Health
    {
        int Value = 0;
    };

    struct Armor
    {
        int Value = 0;
    };

    struct Speed
    {
        float Value = 0.0f;
    };

    // Requires Armor.
    struct ArmorSystem : ECS::System
    {
    };

    // Requires nothing in particular: every entity with any component matches an empty signature.
    struct EverythingSystem : ECS::System
    {
    };

    ECS::ECS MakeEcs()
    {
        ECS::ECS ecs;
        ecs.Init();
        ecs.RegisterComponent<Health>();
        ecs.RegisterComponent<Armor>();
        return ecs;
    }

    bool Contains(const std::vector<ECS::Entity>& entities, ECS::Entity entity)
    {
        return std::find(entities.begin(), entities.end(), entity) != entities.end();
    }
}

TEST_CASE("ECS::UnregisterComponent - every holder's removal callback fires with readable data")
{
    ECS::ECS ecs = MakeEcs();
    std::vector<int> removed;
    ecs.SetComponentRemovedCallback<Health>([&](ECS::Entity, Health& health) { removed.push_back(health.Value); });

    const ECS::Entity a = ecs.CreateEntity();
    const ECS::Entity b = ecs.CreateEntity();
    const ECS::Entity c = ecs.CreateEntity();
    ecs.AddComponent(a, Health{ 10 });
    ecs.AddComponent(b, Health{ 20 });
    ecs.AddComponent(c, Health{ 30 });
    ecs.AddComponent(b, Armor{ 5 });

    REQUIRE(ecs.UnregisterComponent<Health>());

    std::sort(removed.begin(), removed.end());
    CHECK(removed == std::vector<int>{ 10, 20, 30 });
    CHECK_FALSE(ecs.IsComponentRegistered<Health>());
    CHECK(ecs.IsComponentRegistered<Armor>());
    CHECK(ecs.HasComponent<Armor>(b));
    CHECK(ecs.GetComponent<Armor>(b).Value == 5);
    CHECK(ecs.IsAlive(a));
}

TEST_CASE("ECS::UnregisterComponent - the next registered type reuses the freed id without a stale bit")
{
    ECS::ECS ecs = MakeEcs();
    const ECS::ComponentType healthType = ecs.GetComponentType<Health>();

    const ECS::Entity holder = ecs.CreateEntity();
    ecs.AddComponent(holder, Health{ 1 });
    REQUIRE(ecs.UnregisterComponent<Health>());

    ecs.RegisterComponent<Speed>();
    CHECK(ecs.GetComponentType<Speed>() == healthType);
    CHECK_FALSE(ecs.HasComponent<Speed>(holder));

    // A system requiring Speed sees only entities that really hold one.
    auto system = ecs.RegisterSystem<EverythingSystem>();
    ECS::Signature signature;
    signature.set(ecs.GetComponentType<Speed>());
    ecs.SetSystemSignature<EverythingSystem>(signature);
    CHECK(system->GetEntities().empty());

    const ECS::Entity fast = ecs.CreateEntity();
    ecs.AddComponent(fast, Speed{ 2.0f });
    CHECK(system->GetEntities() == std::vector<ECS::Entity>{ fast });
}

TEST_CASE("ECS::UnregisterComponent - the lowest freed id is reused first, then new ids")
{
    ECS::ECS ecs = MakeEcs();
    ecs.RegisterComponent<Speed>();
    const ECS::ComponentType healthType = ecs.GetComponentType<Health>();
    const ECS::ComponentType speedType  = ecs.GetComponentType<Speed>();

    REQUIRE(ecs.UnregisterComponent<Speed>());
    REQUIRE(ecs.UnregisterComponent<Health>());

    ecs.RegisterComponent<Health>();
    ecs.RegisterComponent<Speed>();
    CHECK(ecs.GetComponentType<Health>() == healthType);
    CHECK(ecs.GetComponentType<Speed>() == speedType);
}

TEST_CASE("ECS::UnregisterComponent - refused while a system's signature requires it")
{
    ECS::ECS ecs    = MakeEcs();
    auto     system = ecs.RegisterSystem<ArmorSystem>();
    ECS::Signature signature;
    signature.set(ecs.GetComponentType<Armor>());
    ecs.SetSystemSignature<ArmorSystem>(signature);

    const ECS::Entity armored = ecs.CreateEntity();
    ecs.AddComponent(armored, Armor{ 3 });
    int callbacks = 0;
    ecs.SetComponentRemovedCallback<Armor>([&](ECS::Entity, Armor&) { ++callbacks; });

    CHECK_FALSE(ecs.UnregisterComponent<Armor>());
    CHECK(ecs.IsComponentRegistered<Armor>());
    CHECK(ecs.HasComponent<Armor>(armored));
    CHECK(callbacks == 0);
    CHECK(Contains(system->GetEntities(), armored));

    // Once the system is gone, the component can go.
    REQUIRE(ecs.UnregisterSystem<ArmorSystem>());
    CHECK(ecs.UnregisterComponent<Armor>());
    CHECK(callbacks == 1);
}

TEST_CASE("ECS::UnregisterComponent - a type that is not registered gives false")
{
    ECS::ECS ecs = MakeEcs();
    CHECK_FALSE(ecs.UnregisterComponent<Speed>());
    REQUIRE(ecs.UnregisterComponent<Health>());
    CHECK_FALSE(ecs.UnregisterComponent<Health>());
}

TEST_CASE("ECS::UnregisterComponent - a system that did not require it keeps its members")
{
    ECS::ECS ecs    = MakeEcs();
    auto     system = ecs.RegisterSystem<ArmorSystem>();
    ECS::Signature signature;
    signature.set(ecs.GetComponentType<Armor>());
    ecs.SetSystemSignature<ArmorSystem>(signature);

    const ECS::Entity both = ecs.CreateEntity();
    ecs.AddComponent(both, Armor{ 1 });
    ecs.AddComponent(both, Health{ 2 });

    REQUIRE(ecs.UnregisterComponent<Health>());
    CHECK(system->GetEntities() == std::vector<ECS::Entity>{ both });
}

TEST_CASE("ECS::UnregisterComponent - the type can be registered and used again")
{
    ECS::ECS ecs = MakeEcs();
    const ECS::Entity entity = ecs.CreateEntity();
    ecs.AddComponent(entity, Health{ 1 });
    int callbacks = 0;
    ecs.SetComponentRemovedCallback<Health>([&](ECS::Entity, Health&) { ++callbacks; });
    REQUIRE(ecs.UnregisterComponent<Health>());
    CHECK(callbacks == 1);

    ecs.RegisterComponent<Health>();
    CHECK(ecs.IsComponentRegistered<Health>());
    CHECK_FALSE(ecs.HasComponent<Health>(entity));
    ecs.AddComponent(entity, Health{ 7 });
    CHECK(ecs.GetComponent<Health>(entity).Value == 7);

    // The old removal callback went with the old storage.
    ecs.RemoveComponent<Health>(entity);
    CHECK(callbacks == 1);
}
