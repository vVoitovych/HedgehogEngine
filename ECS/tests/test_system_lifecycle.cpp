#include "doctest/doctest/doctest.h"

#include "ECS/api/ECS.hpp"
#include "ECS/api/ServiceRegistry.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct Health
    {
        int Value = 0;
    };

    // Records its lifecycle into a shared log, prefixed with its name.
    template<int Tag>
    struct LifecycleSystem : ECS::System
    {
        LifecycleSystem(std::string name, std::vector<std::string>& log)
            : Name(std::move(name))
            , Log(log)
        {
            Log.push_back(Name + ".construct");
        }

        void OnRegister(ECS::ECS& ecs) override
        {
            Log.push_back(Name + ".register");
            FoundSelfOnRegister = ecs.GetSystem<LifecycleSystem<Tag>>().get() == this;
        }

        void OnUnregister(ECS::ECS& /*ecs*/) override { Log.push_back(Name + ".unregister"); }

        std::string               Name;
        std::vector<std::string>& Log;
        bool                      FoundSelfOnRegister = false;
    };

    // Reads an entity's component while it is unregistered at teardown.
    struct ComponentReader : ECS::System
    {
        ComponentReader(ECS::Entity entity, int& seen)
            : Watched(entity)
            , Seen(seen)
        {
        }

        void OnUnregister(ECS::ECS& ecs) override { Seen = ecs.GetComponent<Health>(Watched).Value; }

        ECS::Entity Watched;
        int&        Seen;
    };

    // Looks its services up in OnRegister, as engine systems will.
    struct ServiceUser : ECS::System
    {
        void OnRegister(ECS::ECS& ecs) override { Count = &ecs.GetServices().Get<int>(); }

        int* Count = nullptr;
    };

    ECS::ECS MakeEcs()
    {
        ECS::ECS ecs;
        ecs.Init();
        ecs.RegisterComponent<Health>();
        return ecs;
    }
}

TEST_CASE("ECS::System::OnRegister - runs once, after construction, with the system already registered")
{
    ECS::ECS                 ecs = MakeEcs();
    std::vector<std::string> log;

    auto a = ecs.RegisterSystem<LifecycleSystem<1>>("A", log);
    auto b = ecs.RegisterSystem<LifecycleSystem<2>>("B", log);

    CHECK(log == std::vector<std::string>{ "A.construct", "A.register", "B.construct", "B.register" });
    CHECK(a->FoundSelfOnRegister);
    CHECK(b->FoundSelfOnRegister);
}

TEST_CASE("ECS::System::OnUnregister - teardown unregisters every system once, last registered first")
{
    std::vector<std::string> log;
    {
        ECS::ECS ecs = MakeEcs();
        ecs.RegisterSystem<LifecycleSystem<1>>("A", log);
        ecs.RegisterSystem<LifecycleSystem<2>>("B", log);
        ecs.RegisterSystem<LifecycleSystem<3>>("C", log);
        log.clear();
    }
    CHECK(log == std::vector<std::string>{ "C.unregister", "B.unregister", "A.unregister" });
}

TEST_CASE("ECS::System::OnUnregister - components are still readable at teardown")
{
    int seen = -1;
    {
        ECS::ECS          ecs    = MakeEcs();
        const ECS::Entity entity = ecs.CreateEntity();
        ecs.AddComponent(entity, Health{ 42 });
        ecs.RegisterSystem<ComponentReader>(entity, seen);
    }
    CHECK(seen == 42);
}

TEST_CASE("ECS::System::OnUnregister - a moved ECS unregisters its systems once")
{
    std::vector<std::string> log;
    {
        ECS::ECS source = MakeEcs();
        source.RegisterSystem<LifecycleSystem<1>>("A", log);
        log.clear();

        ECS::ECS moved = std::move(source);
        CHECK(log.empty());
    }
    CHECK(log == std::vector<std::string>{ "A.unregister" });
}

TEST_CASE("ECS::System::OnUnregister - move-assigning unregisters the target's own systems first")
{
    std::vector<std::string> log;
    {
        ECS::ECS target = MakeEcs();
        target.RegisterSystem<LifecycleSystem<1>>("Old", log);
        ECS::ECS source = MakeEcs();
        source.RegisterSystem<LifecycleSystem<2>>("New", log);
        log.clear();

        target = std::move(source);
        CHECK(log == std::vector<std::string>{ "Old.unregister" });
        CHECK(target.HasSystem<LifecycleSystem<2>>());
        log.clear();
    }
    CHECK(log == std::vector<std::string>{ "New.unregister" });
}

TEST_CASE("ECS::ServiceRegistry - finds services by type")
{
    ECS::ServiceRegistry services;
    int                  count = 3;
    std::string          name  = "bus";

    services.Register(count);
    services.Register(name);

    CHECK(services.Has<int>());
    CHECK(services.Find<int>() == &count);
    CHECK(&services.Get<std::string>() == &name);
    CHECK(services.Find<float>() == nullptr);
    CHECK_FALSE(services.Has<float>());

    services.Get<int>() = 5;
    CHECK(count == 5);
}

TEST_CASE("ECS::ServiceRegistry - Unregister removes one service and reports whether it was there")
{
    ECS::ServiceRegistry services;
    int                  count = 0;
    std::string          name;
    services.Register(count);
    services.Register(name);

    CHECK(services.Unregister<int>());
    CHECK(services.Find<int>() == nullptr);
    CHECK(services.Find<std::string>() == &name);
    CHECK_FALSE(services.Unregister<int>());

    services.Register(count);
    CHECK(services.Find<int>() == &count);
}

TEST_CASE("ECS::GetServices - kept across Init and reachable from OnRegister")
{
    int      count = 7;
    ECS::ECS ecs;
    ecs.GetServices().Register(count);
    ecs.Init();
    ecs.RegisterComponent<Health>();

    auto user = ecs.RegisterSystem<ServiceUser>();
    CHECK(user->Count == &count);

    const ECS::ECS& constEcs = ecs;
    CHECK(constEcs.GetServices().Find<int>() == &count);
}
