#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Plugins/PluginRegistrar.hpp"
#include "HedgehogEngine/api/Scene/SceneManager.hpp"

#include "EcsSerialization/api/ComponentTypeRegistry.hpp"
#include "EcsSerialization/api/UnknownComponents.hpp"

#include "ECS/api/ECS.hpp"
#include "ECS/api/FrameContext.hpp"
#include "ECS/api/SystemPhase.hpp"

#include "Logger/api/Logger.hpp"

#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace HedgehogEngine;
using EcsSerialization::ComponentDesc;

namespace
{
    // What a plugin would register: a reflected component, a system over it, a service, an event.
    struct PluginThing
    {
        float Value = 1.0f;

        static void* ValueAccessor(void* c) { return &static_cast<PluginThing*>(c)->Value; }

        static std::span<const Reflection::PropertyDescriptor> GetProperties()
        {
            static const Reflection::PropertyDescriptor properties[] = {
                { Reflection::TypeTag::Float, "Value", ValueAccessor, Reflection::PropertyFlags::None, 0.0f, 0.0f },
            };
            return properties;
        }
    };

    struct PluginService
    {
        int Calls = 0;
    };

    struct PluginEvent
    {
        int Amount = 0;
    };

    class PluginThingSystem : public ECS::System
    {
    public:
        ECS::SystemPhase GetPhase() const override { return ECS::SystemPhase::Late; }

        void OnFrame(ECS::ECS& ecs, const ECS::FrameContext&) override
        {
            ++Frames;
            for (const ECS::Entity entity : m_Entities)
                Seen = ecs.GetComponent<PluginThing>(entity).Value;
        }

        int   Frames = 0;
        float Seen   = 0.0f;
    };

    // Collects error lines, since engine tests cannot use HedgehogScripting's log capture.
    struct ErrorLog
    {
        std::vector<std::string> Lines;
        int                      Sink = EngineLogger::Logger::Instance().AddSink(
            [this](EngineLogger::LogLevel level, const std::string& message)
            {
                if (level == EngineLogger::LogLevel::Error)
                    Lines.push_back(message);
            });

        ErrorLog() = default;
        ErrorLog(const ErrorLog&)            = delete;
        ErrorLog& operator=(const ErrorLog&) = delete;
        ~ErrorLog() { EngineLogger::Logger::Instance().RemoveSink(Sink); }
    };

    void RegisterAll(PluginRegistrar& registrar, PluginService& service, int& heard)
    {
        REQUIRE(registrar.RegisterReflectedComponent<PluginThing>(ComponentDesc{ .Key = "PluginThing", .DisplayName = "Thing" }));
        REQUIRE(registrar.RegisterSystem<PluginThingSystem, PluginThing>() != nullptr);
        REQUIRE(registrar.RegisterService(service));
        REQUIRE(registrar.Subscribe<PluginEvent>([&heard](const PluginEvent& event) { heard += event.Amount; }) !=
                SubscriptionId::Invalid);
    }
}

TEST_CASE("PluginRegistrar - a component, a system, a service and a subscription are all registered")
{
    EngineContext   context;
    PluginService   service;
    int             heard = 0;
    PluginRegistrar registrar(context, "TestPlugin");
    RegisterAll(registrar, service, heard);
    CHECK(registrar.GetRegistrationCount() == 4);

    ECS::ECS& ecs = context.GetECS();
    CHECK(context.GetComponentTypes().Find("PluginThing") != nullptr);
    CHECK(ecs.HasSystem<PluginThingSystem>());
    CHECK(ecs.GetServices().Find<PluginService>() == &service);
    CHECK(context.GetEventBus().GetSubscriberCount<PluginEvent>() == 1);

    // The system sees an entity with the component, in its phase of the frame.
    const ECS::Entity entity = context.GetSceneManager().CreateGameObject();
    ecs.AddComponent(entity, PluginThing{ 4.0f });
    context.UpdateContext(1.0f, 1.0f / 60.0f);
    const auto system = ecs.GetSystem<PluginThingSystem>();
    CHECK(system->Frames == 1);
    CHECK(system->Seen == 4.0f);

    context.GetEventBus().Publish(PluginEvent{ 3 });
    CHECK(heard == 3);
}

TEST_CASE("PluginRegistrar - UnregisterAll undoes everything and keeps the component's data")
{
    EngineContext   context;
    ECS::ECS&       ecs = context.GetECS();
    PluginService   service;
    int             heard = 0;
    ErrorLog        errors;
    ECS::Entity     entity = ECS::INVALID_ENTITY;
    std::weak_ptr<PluginThingSystem> system;
    {
        PluginRegistrar registrar(context, "TestPlugin");
        RegisterAll(registrar, service, heard);
        entity = context.GetSceneManager().CreateGameObject();
        ecs.AddComponent(entity, PluginThing{ 7.5f });
        system = ecs.GetSystem<PluginThingSystem>();

        registrar.UnregisterAll();
        CHECK(registrar.GetRegistrationCount() == 0);
        registrar.UnregisterAll(); // twice is harmless
    }
    CHECK(errors.Lines.empty()); // the system went before the component its signature names

    CHECK(context.GetComponentTypes().Find("PluginThing") == nullptr);
    CHECK_FALSE(ecs.IsComponentRegistered<PluginThing>());
    CHECK_FALSE(ecs.HasSystem<PluginThingSystem>());
    CHECK(system.expired()); // nothing holds the system any more
    CHECK_FALSE(ecs.GetServices().Has<PluginService>());
    CHECK(context.GetEventBus().GetSubscriberCount<PluginEvent>() == 0);
    context.GetEventBus().Publish(PluginEvent{ 3 });
    CHECK(heard == 0);

    // The value stayed in the scene, and comes back when the plugin registers again.
    REQUIRE(ecs.HasComponent<EcsSerialization::UnknownComponentsComponent>(entity));
    PluginRegistrar again(context, "TestPlugin");
    RegisterAll(again, service, heard);
    REQUIRE(ecs.HasComponent<PluginThing>(entity));
    CHECK(ecs.GetComponent<PluginThing>(entity).Value == 7.5f);
    CHECK(ecs.GetSystem<PluginThingSystem>()->GetEntities() == std::vector<ECS::Entity>{ entity });
}

TEST_CASE("PluginRegistrar - a refused registration records nothing and does not stop UnregisterAll")
{
    EngineContext   context;
    PluginService   service;
    PluginRegistrar registrar(context, "TestPlugin");
    ErrorLog        errors;

    REQUIRE(registrar.RegisterReflectedComponent<PluginThing>(ComponentDesc{ .Key = "PluginThing" }));
    CHECK_FALSE(registrar.RegisterReflectedComponent<PluginThing>(ComponentDesc{ .Key = "PluginThing" }));
    REQUIRE(registrar.RegisterSystem<PluginThingSystem, PluginThing>() != nullptr);
    CHECK(registrar.RegisterSystem<PluginThingSystem, PluginThing>() == nullptr);
    REQUIRE(registrar.RegisterService(service));
    CHECK_FALSE(registrar.RegisterService(service));
    CHECK(registrar.GetRegistrationCount() == 3);
    CHECK(errors.Lines.size() == 3);

    errors.Lines.clear();
    registrar.UnregisterAll();
    CHECK(errors.Lines.empty());
    CHECK(context.GetComponentTypes().Find("PluginThing") == nullptr);
    CHECK_FALSE(context.GetECS().HasSystem<PluginThingSystem>());
}

TEST_CASE("PluginRegistrar - a registrar that is dropped undoes its registrations")
{
    EngineContext context;
    PluginService service;
    int           heard = 0;
    {
        PluginRegistrar registrar(context, "TestPlugin");
        RegisterAll(registrar, service, heard);
    }
    CHECK(context.GetComponentTypes().Find("PluginThing") == nullptr);
    CHECK_FALSE(context.GetECS().HasSystem<PluginThingSystem>());
    CHECK(context.GetEventBus().GetTotalSubscriberCount() ==
          EngineContext().GetEventBus().GetTotalSubscriberCount()); // only the engine's own remain
}
