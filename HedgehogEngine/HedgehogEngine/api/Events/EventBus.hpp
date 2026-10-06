#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace HedgehogEngine
{
    // Names one subscription, so its owner can unsubscribe. Invalid (0) is never handed out.
    enum class SubscriptionId : uint64_t
    {
        Invalid = 0
    };

    // Typed publish/subscribe, one channel per event struct. Handlers run synchronously, in
    // subscription order. Subscribing or unsubscribing from inside a handler is safe: a handler
    // added during a publish first runs on the next one, and one removed during a publish is not
    // called again, not even later in the same publish.
    //
    // Channels are one concrete, non-virtual type that stores handlers type-erased, so no code of
    // the module that first used an event (a plugin DLL) stays in the bus: once a subscriber has
    // unsubscribed, its module may be unloaded.
    class EventBus
    {
    public:
        // The id is only needed to unsubscribe; a subscriber that lives as long as the bus may
        // ignore it.
        template<typename TEvent>
        SubscriptionId Subscribe(std::function<void(const TEvent&)> handler)
        {
            const SubscriptionId id = static_cast<SubscriptionId>(++m_LastId);
            GetChannel(std::type_index(typeid(TEvent)))
                .Subscribe(id, [handler = std::move(handler)](const void* event) { handler(*static_cast<const TEvent*>(event)); });
            return id;
        }

        // Returns whether a subscription with that id was removed.
        bool Unsubscribe(SubscriptionId id)
        {
            if (id == SubscriptionId::Invalid)
            {
                return false;
            }
            for (auto& channel : m_Channels)
            {
                if (channel.second.Unsubscribe(id))
                {
                    return true;
                }
            }
            return false;
        }

        template<typename TEvent>
        void Publish(const TEvent& event)
        {
            GetChannel(std::type_index(typeid(TEvent))).Publish(&event);
        }

        // How many handlers TEvent has (those subscribed during a publish included, those removed
        // during one excluded).
        template<typename TEvent>
        [[nodiscard]] size_t GetSubscriberCount() const
        {
            const auto found = m_Channels.find(std::type_index(typeid(TEvent)));
            return found == m_Channels.end() ? 0 : found->second.GetSubscriberCount();
        }

        // The live handlers of every channel, counted as GetSubscriberCount does.
        [[nodiscard]] size_t GetTotalSubscriberCount() const
        {
            size_t count = 0;
            for (const auto& channel : m_Channels)
            {
                count += channel.second.GetSubscriberCount();
            }
            return count;
        }

    private:
        class Channel
        {
        public:
            // Takes a pointer to the event, of the type the channel is keyed by.
            using Handler = std::function<void(const void*)>;

            void Subscribe(SubscriptionId id, Handler handler)
            {
                // During a publish the handler list must not grow (a running handler lives in it),
                // so new handlers wait until the outermost publish ends.
                std::vector<Entry>& target = m_PublishDepth > 0 ? m_Pending : m_Handlers;
                target.push_back(Entry{ id, std::move(handler), true });
            }

            bool Unsubscribe(SubscriptionId id)
            {
                for (std::vector<Entry>* list : { &m_Handlers, &m_Pending })
                {
                    for (size_t i = 0; i < list->size(); ++i)
                    {
                        Entry& entry = (*list)[i];
                        if (entry.Id == id && entry.Active)
                        {
                            if (m_PublishDepth > 0)
                            {
                                entry.Active  = false; // erased when the publish ends
                                m_HasRemovals = true;
                            }
                            else
                            {
                                list->erase(list->begin() + static_cast<std::ptrdiff_t>(i));
                            }
                            return true;
                        }
                    }
                }
                return false;
            }

            size_t GetSubscriberCount() const
            {
                size_t count = 0;
                for (const std::vector<Entry>* list : { &m_Handlers, &m_Pending })
                {
                    for (const Entry& entry : *list)
                    {
                        count += entry.Active ? 1 : 0;
                    }
                }
                return count;
            }

            void Publish(const void* event)
            {
                ++m_PublishDepth;
                for (size_t i = 0; i < m_Handlers.size(); ++i)
                {
                    if (m_Handlers[i].Active)
                    {
                        m_Handlers[i].Callback(event);
                    }
                }
                if (--m_PublishDepth == 0)
                {
                    Settle();
                }
            }

        private:
            struct Entry
            {
                SubscriptionId Id = SubscriptionId::Invalid;
                Handler        Callback;
                bool           Active = true;
            };

            // Drops the handlers removed during the publish and adds the ones subscribed during it.
            void Settle()
            {
                if (m_HasRemovals)
                {
                    std::erase_if(m_Handlers, [](const Entry& entry) { return !entry.Active; });
                    std::erase_if(m_Pending, [](const Entry& entry) { return !entry.Active; });
                    m_HasRemovals = false;
                }
                for (Entry& entry : m_Pending)
                {
                    m_Handlers.push_back(std::move(entry));
                }
                m_Pending.clear();
            }

            std::vector<Entry> m_Handlers;
            std::vector<Entry> m_Pending;
            int                m_PublishDepth = 0;
            bool               m_HasRemovals  = false;
        };

        // Map nodes never move, so a channel stays put while a handler publishes another event
        // and the map grows.
        Channel& GetChannel(std::type_index key)
        {
            return m_Channels[key];
        }

        std::unordered_map<std::type_index, Channel> m_Channels;
        uint64_t                                     m_LastId = 0;
    };
}
