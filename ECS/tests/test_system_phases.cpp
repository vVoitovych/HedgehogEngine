#include "doctest/doctest/doctest.h"

#include "ECS/api/ECS.hpp"
#include "ECS/api/FrameContext.hpp"
#include "ECS/api/SystemPhase.hpp"

#include <string>
#include <utility>
#include <vector>

namespace
{
    using Log = std::vector<std::string>;

    // Logs its OnFrame (with the mode it saw) and its play-mode updates, prefixed with its name.
    template<int Tag>
    struct PhaseSystem : ECS::System
    {
        PhaseSystem(std::string name, ECS::SystemPhase phase, Log& log)
            : Name(std::move(name))
            , Phase(phase)
            , Entries(log)
        {
        }

        ECS::SystemPhase GetPhase() const override { return Phase; }

        void OnFrame(ECS::ECS& /*ecs*/, const ECS::FrameContext& ctx) override
        {
            Entries.push_back(Name + ".frame");
            LastContext = ctx;
        }

        void OnFixedUpdate(ECS::ECS& /*ecs*/, float fixedDeltaTime) override
        {
            Entries.push_back(Name + ".fixed");
            LastFixedDeltaTime = fixedDeltaTime;
        }

        void OnPostFixedUpdate(ECS::ECS& ecs, float fixedDeltaTime) override
        {
            Entries.push_back(Name + ".post");
            LastPostFixedDeltaTime = fixedDeltaTime;
            SawDispatchingInPost   = ecs.IsDispatching();
        }

        void OnUpdate(ECS::ECS& /*ecs*/, float deltaTime) override
        {
            Entries.push_back(Name + ".update");
            LastDeltaTime = deltaTime;
        }

        std::string       Name;
        ECS::SystemPhase  Phase;
        Log&              Entries;
        ECS::FrameContext LastContext{};
        float             LastFixedDeltaTime = 0.0f;
        float             LastDeltaTime      = 0.0f;
        float             LastPostFixedDeltaTime = 0.0f;
        bool              SawDispatchingInPost   = false;
    };

    // Overrides nothing of the frame: runs in Late and does nothing there.
    struct DefaultSystem : ECS::System
    {
    };

    // Registers another system from its OnFrame, and records whether the ECS was dispatching.
    struct Registrar : ECS::System
    {
        explicit Registrar(Log& log)
            : Entries(log)
        {
        }

        ECS::SystemPhase GetPhase() const override { return ECS::SystemPhase::Late; }

        void OnFrame(ECS::ECS& ecs, const ECS::FrameContext& /*ctx*/) override
        {
            SawDispatching = ecs.IsDispatching();
            if (!ecs.HasSystem<PhaseSystem<99>>())
            {
                ecs.RegisterSystem<PhaseSystem<99>>("Late2", ECS::SystemPhase::Late, Entries);
            }
        }

        Log& Entries;
        bool SawDispatching = false;
    };

    ECS::ECS MakeEcs()
    {
        ECS::ECS ecs;
        ecs.Init();
        return ecs;
    }

    ECS::FrameContext MakeContext(ECS::PlayMode mode, uint32_t fixedSteps = 0)
    {
        ECS::FrameContext ctx;
        ctx.DeltaTime       = 1.0f / 30.0f;
        ctx.ScaledDeltaTime = 1.0f / 60.0f;
        ctx.FixedDeltaTime  = 1.0f / 120.0f;
        ctx.FixedSteps      = fixedSteps;
        ctx.Mode            = mode;
        return ctx;
    }
}

TEST_CASE("ECS::RunPhases - systems run by phase, then in registration order within a phase")
{
    ECS::ECS ecs = MakeEcs();
    Log      log;
    ecs.RegisterSystem<PhaseSystem<1>>("Late", ECS::SystemPhase::Late, log);
    ecs.RegisterSystem<PhaseSystem<2>>("TransformA", ECS::SystemPhase::Transform, log);
    ecs.RegisterSystem<PhaseSystem<3>>("Input", ECS::SystemPhase::Input, log);
    ecs.RegisterSystem<PhaseSystem<4>>("TransformB", ECS::SystemPhase::Transform, log);
    ecs.RegisterSystem<PhaseSystem<5>>("Sync", ECS::SystemPhase::Sync, log);
    ecs.RegisterSystem<PhaseSystem<6>>("Animation", ECS::SystemPhase::Animation, log);
    ecs.RegisterSystem<PhaseSystem<7>>("Simulation", ECS::SystemPhase::Simulation, log);

    ecs.RunPhases(ECS::SystemPhase::Input, ECS::SystemPhase::Sync, MakeContext(ECS::PlayMode::Edit));

    const Log expected{ "Input.frame",      "Simulation.frame", "Animation.frame", "TransformA.frame",
                        "TransformB.frame", "Late.frame",       "Sync.frame" };
    CHECK(log == expected);
}

TEST_CASE("ECS::RunPhase - Simulation while playing runs the fixed steps and the update before its systems")
{
    ECS::ECS ecs = MakeEcs();
    Log      log;
    // A Late system registered first still gets the play-mode updates first: they go to every
    // system in registration order, as RunFixedUpdate and RunUpdate always have.
    auto late = ecs.RegisterSystem<PhaseSystem<1>>("Late", ECS::SystemPhase::Late, log);
    auto sim  = ecs.RegisterSystem<PhaseSystem<2>>("Sim", ECS::SystemPhase::Simulation, log);

    const ECS::FrameContext ctx = MakeContext(ECS::PlayMode::Playing, 2);
    ecs.RunPhase(ECS::SystemPhase::Simulation, ctx);

    const Log expected{ "Late.fixed", "Sim.fixed", "Late.post", "Sim.post",   "Late.fixed", "Sim.fixed",
                        "Late.post",  "Sim.post",  "Late.update", "Sim.update", "Sim.frame" };
    CHECK(log == expected);
    CHECK(sim->LastFixedDeltaTime == ctx.FixedDeltaTime);
    CHECK(sim->LastDeltaTime == ctx.ScaledDeltaTime);
    CHECK(late->LastDeltaTime == ctx.ScaledDeltaTime);
}

TEST_CASE("ECS::RunFixedUpdate - every system's OnFixedUpdate, then every system's OnPostFixedUpdate, in registration order")
{
    ECS::ECS ecs = MakeEcs();
    Log      log;
    auto     first = ecs.RegisterSystem<PhaseSystem<1>>("A", ECS::SystemPhase::Late, log);
    ecs.RegisterSystem<DefaultSystem>();
    ecs.RegisterSystem<PhaseSystem<2>>("B", ECS::SystemPhase::Simulation, log);
    ecs.RegisterSystem<PhaseSystem<3>>("C", ECS::SystemPhase::Input, log);

    ecs.RunFixedUpdate(0.25f);

    CHECK(log == Log{ "A.fixed", "B.fixed", "C.fixed", "A.post", "B.post", "C.post" });
    CHECK(first->LastPostFixedDeltaTime == 0.25f);
    CHECK(first->SawDispatchingInPost);
    CHECK_FALSE(ecs.IsDispatching());
}

TEST_CASE("ECS::RunPhase - Simulation in Edit or Paused runs only its systems' OnFrame")
{
    for (const ECS::PlayMode mode : { ECS::PlayMode::Edit, ECS::PlayMode::Paused })
    {
        ECS::ECS ecs = MakeEcs();
        Log      log;
        ecs.RegisterSystem<PhaseSystem<1>>("Sim", ECS::SystemPhase::Simulation, log);

        ecs.RunPhase(ECS::SystemPhase::Simulation, MakeContext(mode, 3));
        CHECK(log == Log{ "Sim.frame" });
    }
}

TEST_CASE("ECS::RunPhase - other phases run in every mode and never send play-mode updates")
{
    for (const ECS::PlayMode mode : { ECS::PlayMode::Edit, ECS::PlayMode::Playing, ECS::PlayMode::Paused })
    {
        ECS::ECS ecs = MakeEcs();
        Log      log;
        auto     system = ecs.RegisterSystem<PhaseSystem<1>>("Transform", ECS::SystemPhase::Transform, log);

        const ECS::FrameContext ctx = MakeContext(mode, 2);
        ecs.RunPhase(ECS::SystemPhase::Transform, ctx);
        CHECK(log == Log{ "Transform.frame" });
        CHECK(system->LastContext.Mode == mode);
        CHECK(system->LastContext.DeltaTime == ctx.DeltaTime);
        CHECK(system->LastContext.FixedSteps == 2);
    }
}

TEST_CASE("ECS::RunPhases - runs exactly the inclusive range")
{
    ECS::ECS ecs = MakeEcs();
    Log      log;
    ecs.RegisterSystem<PhaseSystem<1>>("Input", ECS::SystemPhase::Input, log);
    ecs.RegisterSystem<PhaseSystem<2>>("Simulation", ECS::SystemPhase::Simulation, log);
    ecs.RegisterSystem<PhaseSystem<3>>("Animation", ECS::SystemPhase::Animation, log);
    ecs.RegisterSystem<PhaseSystem<4>>("Transform", ECS::SystemPhase::Transform, log);
    ecs.RegisterSystem<PhaseSystem<5>>("Late", ECS::SystemPhase::Late, log);
    ecs.RegisterSystem<PhaseSystem<6>>("Sync", ECS::SystemPhase::Sync, log);

    ecs.RunPhases(ECS::SystemPhase::Animation, ECS::SystemPhase::Late, MakeContext(ECS::PlayMode::Playing, 1));
    CHECK(log == Log{ "Animation.frame", "Transform.frame", "Late.frame" });

    log.clear();
    ecs.RunPhases(ECS::SystemPhase::Sync, ECS::SystemPhase::Sync, MakeContext(ECS::PlayMode::Edit));
    CHECK(log == Log{ "Sync.frame" });
}

TEST_CASE("ECS::RunPhase - a system registered during a dispatch is not visited in that pass")
{
    ECS::ECS ecs = MakeEcs();
    Log      log;
    auto     registrar = ecs.RegisterSystem<Registrar>(log);

    ecs.RunPhase(ECS::SystemPhase::Late, MakeContext(ECS::PlayMode::Edit));
    CHECK(log.empty());
    CHECK(registrar->SawDispatching);

    ecs.RunPhase(ECS::SystemPhase::Late, MakeContext(ECS::PlayMode::Edit));
    CHECK(log == Log{ "Late2.frame" });
}

TEST_CASE("ECS::System - the default phase is Late and the default OnFrame does nothing")
{
    ECS::ECS ecs    = MakeEcs();
    auto     system = ecs.RegisterSystem<DefaultSystem>();
    CHECK(system->GetPhase() == ECS::SystemPhase::Late);

    const ECS::Entity entity = ecs.CreateEntity();
    ecs.RunPhases(ECS::SystemPhase::Input, ECS::SystemPhase::Sync, MakeContext(ECS::PlayMode::Playing, 1));
    CHECK(ecs.IsAlive(entity));
}

TEST_CASE("ECS::IsDispatching - false outside a dispatch")
{
    ECS::ECS ecs = MakeEcs();
    Log      log;
    ecs.RegisterSystem<Registrar>(log);
    CHECK_FALSE(ecs.IsDispatching());

    ecs.RunPhases(ECS::SystemPhase::Input, ECS::SystemPhase::Sync, MakeContext(ECS::PlayMode::Playing, 2));
    ecs.NotifyPlayStart();
    ecs.RunUpdate(0.1f);
    CHECK_FALSE(ecs.IsDispatching());
}
