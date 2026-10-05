#include "doctest/doctest/doctest.h"

#include "ECS/api/ECS.hpp"
#include "ECS/api/FrameContext.hpp"
#include "ECS/api/SystemPhase.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    using Log = std::vector<std::string>;

    struct Tag
    {
        int Value = 0;
    };

    // Logs every call it gets, prefixed with its name.
    template<int Id>
    struct LoggingSystem : ECS::System
    {
        LoggingSystem(std::string name, ECS::SystemPhase phase, Log& log)
            : Name(std::move(name))
            , Phase(phase)
            , Entries(log)
        {
        }

        ECS::SystemPhase GetPhase() const override { return Phase; }
        void OnUnregister(ECS::ECS& /*ecs*/) override { Entries.push_back(Name + ".unregister"); }
        void OnFrame(ECS::ECS& /*ecs*/, const ECS::FrameContext& /*ctx*/) override { Entries.push_back(Name + ".frame"); }
        void OnPlayStart(ECS::ECS& /*ecs*/) override { Entries.push_back(Name + ".start"); }
        void OnUpdate(ECS::ECS& /*ecs*/, float /*dt*/) override { Entries.push_back(Name + ".update"); }

        std::string      Name;
        ECS::SystemPhase Phase;
        Log&             Entries;
    };

    using SystemA = LoggingSystem<1>;
    using SystemB = LoggingSystem<2>;
    using SystemC = LoggingSystem<3>;

    // Tries to unregister SystemA from inside its own frame or update, and records the answer.
    struct InsideUnregisterer : ECS::System
    {
        ECS::SystemPhase GetPhase() const override { return ECS::SystemPhase::Late; }
        void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& /*ctx*/) override { FromFrame = ecs.UnregisterSystem<SystemA>(); }
        void OnUpdate(ECS::ECS& ecs, float /*dt*/) override { FromUpdate = ecs.UnregisterSystem<SystemA>(); }

        bool FromFrame  = true;
        bool FromUpdate = true;
    };

    ECS::ECS MakeEcs()
    {
        ECS::ECS ecs;
        ecs.Init();
        ecs.RegisterComponent<Tag>();
        return ecs;
    }

    ECS::FrameContext Playing()
    {
        ECS::FrameContext ctx;
        ctx.Mode = ECS::PlayMode::Playing;
        return ctx;
    }

    void RunWholeFrame(ECS::ECS& ecs)
    {
        ecs.RunPhases(ECS::SystemPhase::Input, ECS::SystemPhase::Sync, Playing());
    }

    void SetTagSignature(ECS::ECS& ecs)
    {
        ECS::Signature signature;
        signature.set(ecs.GetComponentType<Tag>());
        ecs.SetSystemSignature<SystemA>(signature);
    }
}

TEST_CASE("ECS::UnregisterSystem - the system gets OnUnregister once and stops running")
{
    ECS::ECS ecs = MakeEcs();
    Log      log;
    ecs.RegisterSystem<SystemA>("A", ECS::SystemPhase::Late, log);

    REQUIRE(ecs.UnregisterSystem<SystemA>());
    CHECK(log == Log{ "A.unregister" });
    CHECK_FALSE(ecs.HasSystem<SystemA>());

    log.clear();
    RunWholeFrame(ecs);
    ecs.NotifyPlayStart();
    ecs.RunUpdate(0.1f);
    CHECK(log.empty());
}

TEST_CASE("ECS::UnregisterSystem - the remaining systems keep their relative order")
{
    ECS::ECS ecs = MakeEcs();
    Log      log;
    ecs.RegisterSystem<SystemA>("A", ECS::SystemPhase::Late, log);
    ecs.RegisterSystem<SystemB>("B", ECS::SystemPhase::Late, log);
    ecs.RegisterSystem<SystemC>("C", ECS::SystemPhase::Late, log);

    REQUIRE(ecs.UnregisterSystem<SystemB>());
    log.clear();

    RunWholeFrame(ecs);
    CHECK(log == Log{ "A.update", "C.update", "A.frame", "C.frame" });
}

TEST_CASE("ECS::UnregisterSystem - a system that is not registered gives false")
{
    ECS::ECS ecs = MakeEcs();
    Log      log;
    CHECK_FALSE(ecs.UnregisterSystem<SystemA>());

    ecs.RegisterSystem<SystemA>("A", ECS::SystemPhase::Late, log);
    REQUIRE(ecs.UnregisterSystem<SystemA>());
    CHECK_FALSE(ecs.UnregisterSystem<SystemA>());
    CHECK(log == Log{ "A.unregister" });
}

TEST_CASE("ECS::UnregisterSystem - the type registers again and backfills matching entities")
{
    ECS::ECS          ecs    = MakeEcs();
    Log               log;
    const ECS::Entity tagged = ecs.CreateEntity();
    ecs.AddComponent(tagged, Tag{ 1 });

    ecs.RegisterSystem<SystemA>("A", ECS::SystemPhase::Transform, log);
    SetTagSignature(ecs);
    REQUIRE(ecs.UnregisterSystem<SystemA>());

    auto again = ecs.RegisterSystem<SystemA>("A2", ECS::SystemPhase::Sync, log);
    CHECK(ecs.HasSystem<SystemA>());
    CHECK(again->GetEntities().empty());

    SetTagSignature(ecs);
    CHECK(again->GetEntities() == std::vector<ECS::Entity>{ tagged });

    log.clear();
    RunWholeFrame(ecs);
    CHECK(log == Log{ "A2.update", "A2.frame" });
}

TEST_CASE("ECS::UnregisterSystem - a pointer the caller kept keeps the object, which no longer runs")
{
    ECS::ECS          ecs    = MakeEcs();
    Log               log;
    const ECS::Entity tagged = ecs.CreateEntity();
    ecs.AddComponent(tagged, Tag{ 1 });

    std::shared_ptr<SystemA> kept = ecs.RegisterSystem<SystemA>("A", ECS::SystemPhase::Late, log);
    SetTagSignature(ecs);
    REQUIRE(kept->GetEntities().size() == 1);

    REQUIRE(ecs.UnregisterSystem<SystemA>());
    CHECK(kept.use_count() == 1);
    CHECK(kept->GetEntities().empty());

    // Membership changes no longer reach it.
    const ECS::Entity other = ecs.CreateEntity();
    ecs.AddComponent(other, Tag{ 2 });
    CHECK(kept->GetEntities().empty());

    log.clear();
    RunWholeFrame(ecs);
    CHECK(log.empty());
}

TEST_CASE("ECS::UnregisterSystem - refused while dispatching, changing nothing")
{
    ECS::ECS ecs = MakeEcs();
    Log      log;
    ecs.RegisterSystem<SystemA>("A", ECS::SystemPhase::Late, log);
    auto inside = ecs.RegisterSystem<InsideUnregisterer>();

    RunWholeFrame(ecs);
    CHECK_FALSE(inside->FromUpdate);
    CHECK_FALSE(inside->FromFrame);
    CHECK(ecs.HasSystem<SystemA>());
    CHECK(log == Log{ "A.update", "A.frame" });

    // Between frames it works.
    CHECK(ecs.UnregisterSystem<SystemA>());
}

TEST_CASE("ECS::UnregisterSystem - teardown does not unregister a removed system again")
{
    Log log;
    {
        ECS::ECS ecs = MakeEcs();
        ecs.RegisterSystem<SystemA>("A", ECS::SystemPhase::Late, log);
        ecs.RegisterSystem<SystemB>("B", ECS::SystemPhase::Late, log);
        REQUIRE(ecs.UnregisterSystem<SystemA>());
        log.clear();
    }
    CHECK(log == Log{ "B.unregister" });
}
