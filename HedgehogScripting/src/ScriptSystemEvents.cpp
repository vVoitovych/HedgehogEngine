#include "HedgehogScripting/api/ScriptSystem.hpp"

#include "Bindings/ScriptHandles.hpp"

#include "HedgehogEngine/api/EngineContext.hpp"
#include "HedgehogEngine/api/Events/AnimationEvents.hpp"
#include "HedgehogEngine/api/Events/UiEvents.hpp"

#include "Logger/api/Logger.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Script events: Events.subscribe(name, fn), Events.unsubscribe(id) and Events.publish(name,
// payload). The engine's EventBus is typed per C++ struct and cannot unsubscribe, so script events
// are the ScriptSystem's own state: subscriptions by name and a queue of published events,
// delivered once every script's OnUpdate has run.
namespace HedgehogScripting
{
    void ScriptSystem::RegisterEvents()
    {
        sol::table events = m_Lua.create_named_table("Events");

        // The subscription belongs to the entity whose script is calling, and dies with it.
        events.set_function("subscribe", [this](const std::string& name, sol::protected_function handler)
        {
            const auto owner = m_RunningEntity.has_value() ? m_Scripts.find(*m_RunningEntity) : m_Scripts.end();
            if (owner == m_Scripts.end())
                throw std::runtime_error("Events.subscribe must be called from a running script's method");
            if (!handler.valid())
                throw std::runtime_error("Events.subscribe needs a handler function");

            const uint64_t id = m_NextSubscriptionId++;
            m_Subscriptions[name].push_back(EventSubscription{ id, owner->first, owner->second.Generation, std::move(handler) });
            return id;
        });

        // True when the id named a live subscription.
        events.set_function("unsubscribe", [this](uint64_t id)
        {
            for (auto& [name, subscriptions] : m_Subscriptions)
            {
                const auto it = std::find_if(subscriptions.begin(), subscriptions.end(),
                                             [id](const EventSubscription& s) { return s.Id == id; });
                if (it != subscriptions.end())
                {
                    subscriptions.erase(it);
                    return true;
                }
            }
            return false;
        });

        // Queued; delivered after this frame's OnUpdates, or next frame when published by a handler.
        events.set_function("publish", [this](const std::string& name, sol::object payload)
        {
            m_QueuedEvents.push_back(QueuedEvent{ name, std::move(payload) });
        });
    }

    void ScriptSystem::DispatchEvents()
    {
        // Events a handler publishes land in the emptied queue, for the next frame: no loop.
        for (const QueuedEvent& event : std::exchange(m_QueuedEvents, {}))
        {
            const auto subscribed = m_Subscriptions.find(event.Name);
            if (subscribed == m_Subscriptions.end())
                continue;

            // A handler may (un)subscribe, so walk the ids and look each one up again.
            std::vector<uint64_t> ids;
            for (const EventSubscription& subscription : subscribed->second)
                ids.push_back(subscription.Id);

            for (const uint64_t id : ids)
            {
                auto& subscriptions = m_Subscriptions[event.Name];
                const auto it = std::find_if(subscriptions.begin(), subscriptions.end(),
                                             [id](const EventSubscription& s) { return s.Id == id; });
                if (it == subscriptions.end())
                    continue;
                if (it->Source != ECS::INVALID_ENTITY &&
                    (it->Source != event.Source || it->SourceGeneration != event.SourceGeneration))
                    continue;
                const auto owner = m_Scripts.find(it->Owner);
                if (owner == m_Scripts.end() || owner->second.Generation != it->Generation || owner->second.Faulted)
                    continue;

                // Copies: the handler may change the subscriptions and the scripts.
                const sol::protected_function handler     = it->Handler;
                const sol::table               environment = owner->second.Environment;
                const std::string              entityName  = owner->second.EntityName;
                const std::string              scriptPath  = owner->second.ScriptPath;

                m_RunningEntity = it->Owner;
                try
                {
                    const sol::protected_function_result result = m_Run(environment, handler, event.Payload, event.Name);
                    if (!result.valid())
                    {
                        const sol::error error = result;
                        LogError(entityName, scriptPath, "handler for event '" + event.Name + "': " + error.what());
                    }
                }
                catch (const std::exception& e)
                {
                    LogError(entityName, scriptPath, "handler for event '" + event.Name + "': " + e.what());
                }
                m_RunningEntity.reset();
            }
        }
    }

    void ScriptSystem::QueueAnimationFinished(const HedgehogEngine::AnimationFinishedEvent& event)
    {
        // Published by AnimationSystem after this frame's script hooks: delivered next frame.
        ECS::ECS& ecs = m_Context.GetECS();
        if (!ecs.IsAlive(event.Entity))
            return;
        sol::table payload = m_Lua.create_table_with("entity", Bindings::MakeScriptEntity(ecs, event.Entity),
                                                     "clip", event.Clip);
        m_QueuedEvents.push_back(QueuedEvent{ "AnimationFinished", std::move(payload) });
    }

    void ScriptSystem::QueueButtonClicked(const HedgehogEngine::UiButtonClickedEvent& event)
    {
        ECS::ECS& ecs = m_Context.GetECS();
        if (!ecs.IsAlive(event.Entity))
            return;
        const Bindings::ScriptEntity button  = Bindings::MakeScriptEntity(ecs, event.Entity);
        sol::table                   payload = m_Lua.create_table_with("entity", button);
        m_QueuedEvents.push_back(QueuedEvent{ "UiButtonClicked", std::move(payload), button.Id, button.Generation });
    }

    uint64_t ScriptSystem::SubscribeClick(const Bindings::ScriptEntity& button, sol::protected_function handler)
    {
        const auto owner = m_RunningEntity.has_value() ? m_Scripts.find(*m_RunningEntity) : m_Scripts.end();
        if (owner == m_Scripts.end())
            throw std::runtime_error("onClick must be called from a running script's method");
        if (!handler.valid())
            throw std::runtime_error("onClick needs a handler function");

        const uint64_t id = m_NextSubscriptionId++;
        m_Subscriptions["UiButtonClicked"].push_back(
            EventSubscription{ id, owner->first, owner->second.Generation, std::move(handler), button.Id, button.Generation });
        return id;
    }

    void ScriptSystem::DropSubscriptions(ECS::Entity owner)
    {
        for (auto& [name, subscriptions] : m_Subscriptions)
        {
            subscriptions.erase(std::remove_if(subscriptions.begin(), subscriptions.end(),
                                               [owner](const EventSubscription& s) { return s.Owner == owner; }),
                                subscriptions.end());
        }
    }
}
