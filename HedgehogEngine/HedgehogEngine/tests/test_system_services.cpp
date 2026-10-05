#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Resource/ResourceCatalog.hpp"
#include "HedgehogEngine/api/ECS/systems/HierarchySystem.hpp"
#include "HedgehogEngine/api/ECS/systems/LightSystem.hpp"
#include "HedgehogEngine/api/ECS/systems/TransformSystem.hpp"
#include "HedgehogEngine/api/Events/EventBus.hpp"
#include "HedgehogEngine/api/Events/TransformEvents.hpp"

#include "HedgehogSettings/api/HedgehogSettings.hpp"
#include "HedgehogAudio/api/AudioEngine.hpp"
#include "FileSystem/api/FileSystemManager.hpp"

#include "ECS/api/ECS.hpp"

#include <memory>

using namespace HedgehogEngine;

TEST_CASE("EngineContext - the engine's services are registered in its ECS")
{
    EngineContext                context;
    const ECS::ServiceRegistry& services = context.GetECS().GetServices();

    CHECK(services.Find<EventBus>() == &context.GetEventBus());
    CHECK(services.Find<FS::FileSystemManager>() == &context.GetFileSystem());
    CHECK(services.Find<HA::AudioEngine>() == &context.GetAudioEngine());
    CHECK(services.Find<ResourceCatalog>() == &context.GetResourceCatalog());
    CHECK(services.Find<HedgehogSettings::Settings>() == &context.GetSettings());
}

TEST_CASE("EngineContext - Transform, Hierarchy and Light subscribe once each through the EventBus service")
{
    EngineContext context;
    EventBus&     bus = context.GetEventBus();

    CHECK(bus.GetSubscriberCount<TransformChangedEvent>() == 1);
    CHECK(bus.GetSubscriberCount<LocalMatrixUpdatedEvent>() == 1);
    CHECK(bus.GetSubscriberCount<WorldMatrixUpdatedEvent>() == 1);
}

TEST_CASE("Engine systems - unregistering lets go of the subscription, and events reach no handler")
{
    EventBus bus;
    {
        ECS::ECS ecs;
        ecs.Init();
        ecs.GetServices().Register(bus);
        ecs.RegisterSystem<TransformSystem>();
        ecs.RegisterSystem<HierarchySystem>();
        auto light = ecs.RegisterSystem<LightSystem>();
        CHECK(bus.GetSubscriberCount<WorldMatrixUpdatedEvent>() == 1);

        // Unregistered while the caller keeps the object: it no longer hears the event.
        REQUIRE(ecs.UnregisterSystem<LightSystem>());
        CHECK(bus.GetSubscriberCount<WorldMatrixUpdatedEvent>() == 0);
        CHECK(bus.GetSubscriberCount<TransformChangedEvent>() == 1);
    }

    // The ECS is gone; its systems unsubscribed at teardown, so publishing calls nothing dangling.
    CHECK(bus.GetSubscriberCount<TransformChangedEvent>() == 0);
    CHECK(bus.GetSubscriberCount<LocalMatrixUpdatedEvent>() == 0);
    bus.Publish(TransformChangedEvent{ 0 });
    bus.Publish(LocalMatrixUpdatedEvent{ 0 });
    bus.Publish(WorldMatrixUpdatedEvent{ 0 });
}

TEST_CASE("Engine systems - without an EventBus service they register and subscribe to nothing")
{
    ECS::ECS ecs;
    ecs.Init();
    auto light = ecs.RegisterSystem<LightSystem>();
    CHECK(ecs.HasSystem<LightSystem>());
    CHECK(ecs.UnregisterSystem<LightSystem>());
}
