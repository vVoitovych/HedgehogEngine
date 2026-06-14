#pragma once

#include <functional>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace HedgehogEngine
{
    class EventBus
    {
    public:
        template<typename TEvent>
        void Subscribe(std::function<void(const TEvent&)> handler)
        {
            GetChannel<TEvent>().Subscribe(std::move(handler));
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
        };

        template<typename TEvent>
        class Channel : public IChannel
        {
        public:
            void Subscribe(std::function<void(const TEvent&)> h)
            {
                m_Handlers.push_back(std::move(h));
            }

            void Publish(const TEvent& e)
            {
                for (auto& h : m_Handlers)
                    h(e);
            }

        private:
            std::vector<std::function<void(const TEvent&)>> m_Handlers;
        };

        template<typename TEvent>
        Channel<TEvent>& GetChannel()
        {
            const auto key = std::type_index(typeid(TEvent));
            auto       it  = m_Channels.find(key);
            if (it == m_Channels.end())
                it = m_Channels.emplace(key, std::make_unique<Channel<TEvent>>()).first;
            return *static_cast<Channel<TEvent>*>(it->second.get());
        }

        std::unordered_map<std::type_index, std::unique_ptr<IChannel>> m_Channels;
    };
}
