#include "doctest/doctest/doctest.h"

#include "ECS/api/ECS.hpp"
#include "ECS/api/HierarchyFunctions.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
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

    struct LateSystem : ECS::System
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

// ---------------------------------------------------------------------------
// Component-removed callbacks
// ---------------------------------------------------------------------------

namespace
{
    // Records each callback: the entity, and the data it could read at the time.
    struct RemovalLog
    {
        std::vector<ECS::Entity> Entities;
        std::vector<float>       Xs;
    };

    void RecordPositionRemovals(ECS::ECS& ecs, RemovalLog& log)
    {
        ecs.SetComponentRemovedCallback<Position>([&log](ECS::Entity entity, Position& position)
        {
            log.Entities.push_back(entity);
            log.Xs.push_back(position.x);
        });
    }

    // The same teardown SceneManager::DeleteGameObjectAndChildren runs.
    void DestroyWithChildren(ECS::ECS& ecs, ECS::Entity entity)
    {
        const std::vector<ECS::Entity> children = ECS::GetChildren(ecs, entity);
        for (ECS::Entity child : children)
            DestroyWithChildren(ecs, child);
        ecs.DestroyEntity(entity);
    }
}

TEST_CASE("ECS removed callback - fires once on RemoveComponent with the data readable")
{
    ECS::ECS ecs = MakeEcs();
    RemovalLog log;
    RecordPositionRemovals(ecs, log);

    const ECS::Entity e = ecs.CreateEntity();
    ecs.AddComponent(e, Position{ 7.0f, 0.0f, 0.0f });
    ecs.AddComponent(e, Velocity{});

    ecs.RemoveComponent<Velocity>(e);
    CHECK(log.Entities.empty()); // only Position has a callback

    ecs.RemoveComponent<Position>(e);
    REQUIRE(log.Entities.size() == 1u);
    CHECK(log.Entities[0] == e);
    CHECK(log.Xs[0] == 7.0f);
}

TEST_CASE("ECS removed callback - fires once on DestroyEntity, while the entity is alive")
{
    ECS::ECS ecs = MakeEcs();

    std::vector<bool> aliveDuringCallback;
    std::vector<bool> hadComponentDuringCallback;
    ecs.SetComponentRemovedCallback<Position>([&](ECS::Entity entity, Position&)
    {
        aliveDuringCallback.push_back(ecs.IsAlive(entity));
        hadComponentDuringCallback.push_back(ecs.HasComponent<Position>(entity));
    });

    const ECS::Entity e = ecs.CreateEntity();
    ecs.AddComponent(e, Position{ 3.0f, 0.0f, 0.0f });
    ecs.DestroyEntity(e);

    REQUIRE(aliveDuringCallback.size() == 1u);
    CHECK(aliveDuringCallback[0]);
    CHECK(hadComponentDuringCallback[0]);
    CHECK_FALSE(ecs.HasComponent<Position>(e));

    // An entity without the component triggers nothing.
    const ECS::Entity bare = ecs.CreateEntity();
    ecs.DestroyEntity(bare);
    CHECK(aliveDuringCallback.size() == 1u);
}

TEST_CASE("ECS removed callback - swap-remove hands each callback its own data")
{
    ECS::ECS ecs = MakeEcs();
    RemovalLog log;
    RecordPositionRemovals(ecs, log);

    std::vector<ECS::Entity> entities;
    for (int i = 0; i < 5; ++i)
    {
        const ECS::Entity e = ecs.CreateEntity();
        ecs.AddComponent(e, Position{ static_cast<float>(i), 0.0f, 0.0f });
        entities.push_back(e);
    }

    // Removing the first element moves the last one into its slot.
    ecs.DestroyEntity(entities[0]);
    ecs.RemoveComponent<Position>(entities[4]);
    ecs.DestroyEntity(entities[2]);

    REQUIRE(log.Xs.size() == 3u);
    CHECK(log.Xs[0] == 0.0f);
    CHECK(log.Xs[1] == 4.0f);
    CHECK(log.Xs[2] == 2.0f);
}

TEST_CASE("ECS removed callback - fires once per entity in a whole-hierarchy teardown")
{
    ECS::ECS ecs = MakeEcs();
    ecs.RegisterComponent<ECS::HierarchyComponent>();
    RemovalLog log;
    RecordPositionRemovals(ecs, log);

    // root -> a -> (a1, a2), root -> b
    const ECS::Entity root = ecs.CreateEntity();
    const ECS::Entity a    = ecs.CreateEntity();
    const ECS::Entity a1   = ecs.CreateEntity();
    const ECS::Entity a2   = ecs.CreateEntity();
    const ECS::Entity b    = ecs.CreateEntity();
    ecs.AddComponent(root, ECS::HierarchyComponent{ "root", root, { a, b } });
    ecs.AddComponent(a,    ECS::HierarchyComponent{ "a",    root, { a1, a2 } });
    ecs.AddComponent(a1,   ECS::HierarchyComponent{ "a1",   a,    {} });
    ecs.AddComponent(a2,   ECS::HierarchyComponent{ "a2",   a,    {} });
    ecs.AddComponent(b,    ECS::HierarchyComponent{ "b",    root, {} });

    const std::map<ECS::Entity, float> xs{ { root, 0.0f }, { a, 1.0f }, { a1, 2.0f }, { a2, 3.0f }, { b, 4.0f } };
    for (auto const& [entity, x] : xs)
        ecs.AddComponent(entity, Position{ x, 0.0f, 0.0f });

    DestroyWithChildren(ecs, root);

    REQUIRE(log.Entities.size() == xs.size());
    std::map<ECS::Entity, int> calls;
    for (size_t i = 0; i < log.Entities.size(); ++i)
    {
        ++calls[log.Entities[i]];
        CHECK(log.Xs[i] == xs.at(log.Entities[i]));
    }
    for (auto const& [entity, x] : xs)
    {
        CHECK(calls[entity] == 1);
        CHECK_FALSE(ecs.IsAlive(entity));
    }
}

TEST_CASE("ECS removed callback - a new callback replaces the old, an empty one clears it")
{
    ECS::ECS ecs = MakeEcs();
    int first  = 0;
    int second = 0;
    ecs.SetComponentRemovedCallback<Position>([&first](ECS::Entity, Position&) { ++first; });
    ecs.SetComponentRemovedCallback<Position>([&second](ECS::Entity, Position&) { ++second; });

    const ECS::Entity e = ecs.CreateEntity();
    ecs.AddComponent(e, Position{});
    ecs.RemoveComponent<Position>(e);
    CHECK(first == 0);
    CHECK(second == 1);

    ecs.SetComponentRemovedCallback<Position>({});
    ecs.AddComponent(e, Position{});
    ecs.RemoveComponent<Position>(e);
    CHECK(second == 1);
}

// ---------------------------------------------------------------------------
// Entity generations
// ---------------------------------------------------------------------------

TEST_CASE("ECS::IsAlive - follows create and destroy; unknown ids are not alive")
{
    ECS::ECS ecs;
    ecs.Init();

    CHECK_FALSE(ecs.IsAlive(ECS::INVALID_ENTITY));
    CHECK_FALSE(ecs.IsAlive(ECS::MAX_ENTITIES));

    const ECS::Entity e = ecs.CreateEntity();
    CHECK(ecs.IsAlive(e));

    const ECS::Entity reserved = 42;
    CHECK_FALSE(ecs.IsAlive(reserved));
    ecs.CreateEntity(reserved);
    CHECK(ecs.IsAlive(reserved));

    ecs.DestroyEntity(e);
    CHECK_FALSE(ecs.IsAlive(e));
    CHECK(ecs.IsAlive(reserved));
}

TEST_CASE("ECS::GetGeneration - a recycled id reports a different generation")
{
    ECS::ECS ecs;
    ecs.Init();

    const ECS::Entity e          = ecs.CreateEntity();
    const uint32_t    generation = ecs.GetGeneration(e);

    ecs.DestroyEntity(e);
    CHECK(ecs.GetGeneration(e) != generation);

    // The pool is LIFO, so the next id handed out is the one just destroyed.
    const ECS::Entity recycled = ecs.CreateEntity();
    REQUIRE(recycled == e);
    CHECK(ecs.IsAlive(recycled));
    CHECK(ecs.GetGeneration(recycled) != generation);

    const uint32_t secondGeneration = ecs.GetGeneration(recycled);
    ecs.DestroyEntity(recycled);
    CHECK(ecs.GetGeneration(recycled) != secondGeneration);
    CHECK(ecs.GetGeneration(recycled) != generation);
}

// ---------------------------------------------------------------------------
// Late system signature (backfill)
// ---------------------------------------------------------------------------

TEST_CASE("ECS::SetSystemSignature - backfills live entities that already match")
{
    ECS::ECS ecs = MakeEcs();

    const ECS::Entity both     = ecs.CreateEntity();
    const ECS::Entity onlyPos  = ecs.CreateEntity();
    const ECS::Entity departed = ecs.CreateEntity();
    ecs.CreateEntity(); // a live entity with no components
    ecs.AddComponent(both, Position{});
    ecs.AddComponent(both, Velocity{});
    ecs.AddComponent(onlyPos, Position{});
    ecs.AddComponent(departed, Position{});
    ecs.AddComponent(departed, Velocity{});
    ecs.DestroyEntity(departed);

    auto system = ecs.RegisterSystem<LateSystem>();
    ECS::Signature signature;
    signature.set(ecs.GetComponentType<Position>());
    signature.set(ecs.GetComponentType<Velocity>());
    ecs.SetSystemSignature<LateSystem>(signature);

    REQUIRE(system->GetEntities().size() == 1u);
    CHECK(system->GetEntities().front() == both);

    // Membership keeps following signatures after the backfill.
    ecs.AddComponent(onlyPos, Velocity{});
    CHECK(system->GetEntities().size() == 2u);
}

TEST_CASE("ECS::SetSystemSignature - setting it again re-evaluates membership")
{
    ECS::ECS ecs = MakeEcs();

    const ECS::Entity posOnly = ecs.CreateEntity();
    const ECS::Entity velOnly = ecs.CreateEntity();
    ecs.AddComponent(posOnly, Position{});
    ecs.AddComponent(velOnly, Velocity{});

    auto system = ecs.RegisterSystem<LateSystem>();
    ECS::Signature signature;
    signature.set(ecs.GetComponentType<Position>());
    ecs.SetSystemSignature<LateSystem>(signature);
    REQUIRE(system->GetEntities().size() == 1u);
    CHECK(system->GetEntities().front() == posOnly);

    signature.reset();
    signature.set(ecs.GetComponentType<Velocity>());
    ecs.SetSystemSignature<LateSystem>(signature);
    REQUIRE(system->GetEntities().size() == 1u);
    CHECK(system->GetEntities().front() == velOnly);
}

TEST_CASE("ECS removed callback - on DestroyEntity every callback still sees the entity's other components")
{
    ECS::ECS ecs = MakeEcs();

    const ECS::Entity e = ecs.CreateEntity();
    ecs.AddComponent(e, Position{ 1.0f, 2.0f, 3.0f });
    ecs.AddComponent(e, Velocity{ 4.0f, 5.0f, 6.0f });

    // Whichever array is notified first, the other component must still be there.
    bool positionSawVelocity = false;
    bool velocitySawPosition = false;
    ecs.SetComponentRemovedCallback<Position>([&](ECS::Entity entity, Position&)
    {
        positionSawVelocity = ecs.HasComponent<Velocity>(entity) &&
                              ecs.GetComponent<Velocity>(entity).dx == 4.0f;
    });
    ecs.SetComponentRemovedCallback<Velocity>([&](ECS::Entity entity, Velocity&)
    {
        velocitySawPosition = ecs.HasComponent<Position>(entity) &&
                              ecs.GetComponent<Position>(entity).x == 1.0f;
    });

    ecs.DestroyEntity(e);

    CHECK(positionSawVelocity);
    CHECK(velocitySawPosition);
    CHECK_FALSE(ecs.IsAlive(e));
}

// ---------------------------------------------------------------------------
// Systems: construction arguments, registration order and play-mode events
// ---------------------------------------------------------------------------

namespace
{
    // Logs every hook as "<name>.<hook>"; the ECS keeps one system per type, so each tag is its
    // own system.
    template<int Tag>
    class RecordingSystem : public ECS::System
    {
    public:
        RecordingSystem(std::string name, std::vector<std::string>& log)
            : Name(std::move(name)), Log(log)
        {
        }

        void OnPlayStart(ECS::ECS&) override  { Log.push_back(Name + ".start"); }
        void OnPlayPause(ECS::ECS&) override  { Log.push_back(Name + ".pause"); }
        void OnPlayResume(ECS::ECS&) override { Log.push_back(Name + ".resume"); }
        void OnPlayStop(ECS::ECS&) override   { Log.push_back(Name + ".stop"); }
        void OnFixedUpdate(ECS::ECS&, float fixedDeltaTime) override
        {
            Log.push_back(Name + ".fixed");
            LastFixedDeltaTime = fixedDeltaTime;
        }
        void OnUpdate(ECS::ECS&, float deltaTime) override
        {
            Log.push_back(Name + ".update");
            LastDeltaTime = deltaTime;
        }

        std::string               Name;
        std::vector<std::string>& Log;
        float                     LastFixedDeltaTime = 0.0f;
        float                     LastDeltaTime      = 0.0f;
    };

    // Overrides nothing.
    class PlainSystem : public ECS::System
    {
    };

    // Uses the component storage from its destructor, as a system clearing its removal
    // callback would.
    class CallbackOwner : public ECS::System
    {
    public:
        explicit CallbackOwner(ECS::ECS& ecs)
            : m_Ecs(&ecs)
        {
            m_Ecs->SetComponentRemovedCallback<Position>([](ECS::Entity, Position&) {});
        }
        ~CallbackOwner() override
        {
            m_Ecs->SetComponentRemovedCallback<Position>({});
            DestroyedWithStorageAlive = true;
        }

        static inline bool DestroyedWithStorageAlive = false;

    private:
        ECS::ECS* m_Ecs;
    };
}

TEST_CASE("ECS::RegisterSystem - forwards constructor arguments")
{
    ECS::ECS                 ecs = MakeEcs();
    std::vector<std::string> log;

    auto system = ecs.RegisterSystem<RecordingSystem<1>>("first", log);
    REQUIRE(system.get() != nullptr);
    CHECK(system->Name == "first");
    CHECK(&system->Log == &log);
    CHECK(ecs.GetSystem<RecordingSystem<1>>().get() == system.get());
}

TEST_CASE("ECS play events - each reaches every system in registration order, stop in reverse")
{
    ECS::ECS                 ecs = MakeEcs();
    std::vector<std::string> log;
    ecs.RegisterSystem<RecordingSystem<1>>("A", log);
    ecs.RegisterSystem<PlainSystem>();
    auto b = ecs.RegisterSystem<RecordingSystem<2>>("B", log);

    ecs.NotifyPlayStart();
    ecs.RunFixedUpdate(1.0f / 60.0f);
    ecs.RunUpdate(1.0f / 30.0f);
    ecs.NotifyPlayPause();
    ecs.NotifyPlayResume();
    ecs.NotifyPlayStop();

    const std::vector<std::string> expected{ "A.start",  "B.start",  "A.fixed",  "B.fixed",
                                             "A.update", "B.update", "A.pause",  "B.pause",
                                             "A.resume", "B.resume", "B.stop",   "A.stop" };
    CHECK(log == expected);
    CHECK(b->LastFixedDeltaTime == 1.0f / 60.0f);
    CHECK(b->LastDeltaTime == 1.0f / 30.0f);
}

TEST_CASE("ECS play events - the ECS keeps no play state and forwards every call as it comes")
{
    // Deciding when an event is due belongs to whoever drives play mode, not to the ECS.
    ECS::ECS                 ecs = MakeEcs();
    std::vector<std::string> log;
    ecs.RegisterSystem<RecordingSystem<1>>("A", log);

    ecs.RunUpdate(0.5f);
    ecs.NotifyPlayStop();
    ecs.NotifyPlayStop();
    CHECK(log == std::vector<std::string>{ "A.update", "A.stop", "A.stop" });
}

TEST_CASE("ECS::System - a system that overrides no event is left untouched")
{
    ECS::ECS ecs   = MakeEcs();
    auto     plain = ecs.RegisterSystem<PlainSystem>();

    const ECS::Entity e = ecs.CreateEntity();
    ecs.NotifyPlayStart();
    ecs.RunFixedUpdate(1.0f / 60.0f);
    ecs.RunUpdate(1.0f / 60.0f);
    ecs.NotifyPlayPause();
    ecs.NotifyPlayResume();
    ecs.NotifyPlayStop();

    CHECK(ecs.IsAlive(e));
    CHECK(plain->GetEntities().empty());
}

TEST_CASE("ECS teardown - systems are destroyed while the component storage still exists")
{
    CallbackOwner::DestroyedWithStorageAlive = false;
    {
        auto ecs = std::make_unique<ECS::ECS>();
        ecs->Init();
        ecs->RegisterComponent<Position>();
        ecs->RegisterSystem<CallbackOwner>(*ecs);
    }
    CHECK(CallbackOwner::DestroyedWithStorageAlive);
}
