#include "doctest/doctest/doctest.h"

#include "HedgehogEngine/api/Events/EventBus.hpp"

#include <string>
#include <vector>

using namespace HedgehogEngine;

namespace
{
    struct Ping
    {
        int Value = 0;
    };

    struct Pong
    {
        int Value = 0;
    };

    using Log = std::vector<std::string>;
}

TEST_CASE("EventBus - handlers run in subscription order with the event")
{
    EventBus bus;
    Log      log;
    bus.Subscribe<Ping>([&](const Ping& ping) { log.push_back("a" + std::to_string(ping.Value)); });
    bus.Subscribe<Ping>([&](const Ping& ping) { log.push_back("b" + std::to_string(ping.Value)); });
    bus.Subscribe<Pong>([&](const Pong&) { log.push_back("pong"); });

    bus.Publish(Ping{ 7 });
    CHECK(log == Log{ "a7", "b7" });
}

TEST_CASE("EventBus::Subscribe - every subscription gets its own valid id")
{
    EventBus             bus;
    const SubscriptionId a = bus.Subscribe<Ping>([](const Ping&) {});
    const SubscriptionId b = bus.Subscribe<Ping>([](const Ping&) {});
    const SubscriptionId c = bus.Subscribe<Pong>([](const Pong&) {});
    CHECK(a != SubscriptionId::Invalid);
    CHECK(a != b);
    CHECK(b != c);
    CHECK(a != c);
}

TEST_CASE("EventBus::Unsubscribe - the handler receives nothing while the others still do")
{
    EventBus             bus;
    Log                  log;
    const SubscriptionId a = bus.Subscribe<Ping>([&](const Ping&) { log.push_back("a"); });
    bus.Subscribe<Ping>([&](const Ping&) { log.push_back("b"); });

    CHECK(bus.Unsubscribe(a));
    bus.Publish(Ping{});
    CHECK(log == Log{ "b" });
}

TEST_CASE("EventBus::Unsubscribe - unknown, invalid and already removed ids give false")
{
    EventBus             bus;
    const SubscriptionId id = bus.Subscribe<Ping>([](const Ping&) {});

    CHECK_FALSE(bus.Unsubscribe(SubscriptionId::Invalid));
    CHECK_FALSE(bus.Unsubscribe(static_cast<SubscriptionId>(999)));
    CHECK(bus.Unsubscribe(id));
    CHECK_FALSE(bus.Unsubscribe(id));
}

TEST_CASE("EventBus::Unsubscribe - a handler removing itself is not called again")
{
    EventBus       bus;
    Log            log;
    SubscriptionId self = SubscriptionId::Invalid;
    self = bus.Subscribe<Ping>([&](const Ping&)
    {
        log.push_back("self");
        CHECK(bus.Unsubscribe(self));
    });
    bus.Subscribe<Ping>([&](const Ping&) { log.push_back("other"); });

    bus.Publish(Ping{});
    bus.Publish(Ping{});
    CHECK(log == Log{ "self", "other", "other" });
}

TEST_CASE("EventBus::Unsubscribe - a handler removed by an earlier one is skipped in the same publish")
{
    EventBus       bus;
    Log            log;
    SubscriptionId later = SubscriptionId::Invalid;
    bus.Subscribe<Ping>([&](const Ping&)
    {
        log.push_back("first");
        bus.Unsubscribe(later);
    });
    later = bus.Subscribe<Ping>([&](const Ping&) { log.push_back("later"); });
    bus.Subscribe<Ping>([&](const Ping&) { log.push_back("last"); });

    bus.Publish(Ping{});
    CHECK(log == Log{ "first", "last" });
}

TEST_CASE("EventBus::Subscribe - a handler added during a publish first runs on the next one")
{
    EventBus bus;
    Log      log;
    bool     added = false;
    // Enough subscriptions that a growing handler list would reallocate under the running one.
    for (int i = 0; i < 8; ++i)
    {
        bus.Subscribe<Ping>([&](const Ping&)
        {
            if (!added)
            {
                added = true;
                for (int j = 0; j < 32; ++j)
                {
                    bus.Subscribe<Ping>([&](const Ping&) { log.push_back("new"); });
                }
            }
        });
    }

    bus.Publish(Ping{});
    CHECK(log.empty());
    bus.Publish(Ping{});
    CHECK(log.size() == 32);
}

TEST_CASE("EventBus::Unsubscribe - a handler added and removed in the same publish never runs")
{
    EventBus       bus;
    Log            log;
    SubscriptionId added = SubscriptionId::Invalid;
    bus.Subscribe<Ping>([&](const Ping&)
    {
        if (added == SubscriptionId::Invalid)
        {
            added = bus.Subscribe<Ping>([&](const Ping&) { log.push_back("added"); });
            CHECK(bus.Unsubscribe(added));
        }
    });

    bus.Publish(Ping{});
    bus.Publish(Ping{});
    CHECK(log.empty());
}

TEST_CASE("EventBus - a handler may publish other events and the same event again")
{
    EventBus bus;
    Log      log;
    bus.Subscribe<Ping>([&](const Ping& ping)
    {
        log.push_back("ping" + std::to_string(ping.Value));
        if (ping.Value == 0)
        {
            bus.Publish(Pong{});
            bus.Publish(Ping{ 1 });
        }
    });
    bus.Subscribe<Pong>([&](const Pong&) { log.push_back("pong"); });

    bus.Publish(Ping{ 0 });
    CHECK(log == Log{ "ping0", "pong", "ping1" });
}

TEST_CASE("EventBus::Unsubscribe - works across channels from inside a handler")
{
    EventBus             bus;
    Log                  log;
    const SubscriptionId pong = bus.Subscribe<Pong>([&](const Pong&) { log.push_back("pong"); });
    bus.Subscribe<Ping>([&](const Ping&) { CHECK(bus.Unsubscribe(pong)); });

    bus.Publish(Ping{});
    bus.Publish(Pong{});
    CHECK(log.empty());
}
