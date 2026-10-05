#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
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
    class EventBus
    {
    public:
        // The id is only needed to unsubscribe; a subscriber that lives as long as the bus may
        // ignore it.
        template<typename TEvent>
        SubscriptionId Subscribe(std::function<void(const TEvent&)> handler)
        {
            const SubscriptionId id = static_cast<SubscriptionId>(++m_LastId);
            GetChannel<TEvent>().Subscribe(id, std::move(handler));
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
                if (channel.second->Unsubscribe(id))
                {
                    return true;
                }
            }
            return false;
        }

        template<typename TEvent>
        void Publish(const TEvent& event)
        {
            GetChannel<TEvent>().Publish(event);
        }

    private:
        class IChannel
        {
        public:
            virtual ~IChannel() = default;
            virtual bool Unsubscribe(SubscriptionId id) = 0;
        };

        template<typename TEvent>
        class Channel : public IChannel
        {
        public:
            using Handler = std::function<void(const TEvent&)>;

            void Subscribe(SubscriptionId id, Handler handler)
            {
                // During a publish the handler list must not grow (a running handler lives in it),
                // so new handlers wait until the outermost publish ends.
                std::vector<Entry>& target = m_PublishDepth > 0 ? m_Pending : m_Handlers;
                target.push_back(Entry{ id, std::move(handler), true });
            }

            bool Unsubscribe(SubscriptionId id) override
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
                                entry.Active   = false; // erased when the publish ends
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

            void Publish(const TEvent& event)
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

        template<typename TEvent>
        Channel<TEvent>& GetChannel()
        {
            const auto key = std::type_index(typeid(TEvent));
            auto       it  = m_Channels.find(key);
            if (it == m_Channels.end())
            {
                it = m_Channels.emplace(key, std::make_unique<Channel<TEvent>>()).first;
            }
            return *static_cast<Channel<TEvent>*>(it->second.get());
        }

        std::unordered_map<std::type_index, std::unique_ptr<IChannel>> m_Channels;
        uint64_t                                                       m_LastId = 0;
    };
}
