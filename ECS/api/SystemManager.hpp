#pragma once

#include "Entity.hpp"
#include "System.hpp"

#include <unordered_map>
#include <memory>
#include <cassert>
#include <algorithm>
#include <array>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace ECS
{
    class SystemManager
    {
    public:
        // Constructs T from args. Systems are also kept in registration order, which is the
        // order ForEachSystem visits them in.
        template<typename T, typename... Args>
        std::shared_ptr<T> RegisterSystem(Args&&... args)
        {
            static_assert(std::is_base_of_v<System, T>, "A system must derive from ECS::System.");
            const std::type_index typeId = typeid(T);
            assert(m_Systems.find(typeId) == m_Systems.end() &&
                "System already registered!");

            auto system = std::make_shared<T>(std::forward<Args>(args)...);
            m_Systems.insert({ typeId, system });
            m_Order.push_back(system);
            m_Phases[static_cast<size_t>(system->GetPhase())].push_back(system);
            return system;
        }

        // Calls fn(System&) for every system in registration order. A system registered while
        // visiting is not visited this time.
        template<typename Fn>
        void ForEachSystem(Fn&& fn) const
        {
            const size_t count = m_Order.size();
            for (size_t i = 0; i < count; ++i)
                fn(*m_Order[i]);
        }

        // Calls fn(System&) for every system of one phase in registration order. A system
        // registered while visiting is not visited this time.
        template<typename Fn>
        void ForEachSystemInPhase(SystemPhase phase, Fn&& fn) const
        {
            const std::vector<std::shared_ptr<System>>& systems = m_Phases[static_cast<size_t>(phase)];
            const size_t                                count   = systems.size();
            for (size_t i = 0; i < count; ++i)
            {
                fn(*systems[i]);
            }
        }

        // The same as ForEachSystem, last registered first.
        template<typename Fn>
        void ForEachSystemReverse(Fn&& fn) const
        {
            for (size_t i = m_Order.size(); i > 0; --i)
                fn(*m_Order[i - 1]);
        }

        template<typename T>
        void SetSignature(Signature signature)
        {
            const std::type_index typeId = typeid(T);
            assert(m_Systems.find(typeId) != m_Systems.end() &&
                "System used before being registered!");

            m_Signatures.insert_or_assign(typeId, signature);
        }

        // Adds or removes one entity from system T to match T's signature.
        template<typename T>
        void EntityChangedSignatureFor(Entity entity, Signature signature)
        {
            const std::type_index typeId = typeid(T);
            assert(m_Systems.find(typeId) != m_Systems.end() &&
                "System used before being registered!");

            UpdateMembership(*m_Systems.at(typeId), m_Signatures[typeId], entity, signature);
        }

        template<typename T>
        bool HasSystem() const
        {
            const std::type_index typeId = typeid(T);
            return m_Systems.find(typeId) != m_Systems.end();
        }

        template<typename T>
        std::shared_ptr<T> GetSystem() const
        {
            const std::type_index typeId = typeid(T);
            assert(m_Systems.find(typeId) != m_Systems.end() &&
                "System used before being registered!");

            return std::static_pointer_cast<T>(m_Systems.at(typeId));
        }

        void EntityDestroyed(Entity entity)
        {
            for (auto const& pair : m_Systems)
            {
                auto const& system = pair.second;
                auto it = std::find(system->m_Entities.begin(), system->m_Entities.end(), entity);
                if (it != system->m_Entities.end())
                {
                    system->m_Entities.erase(it);
                }
            }
        }

        void EntityChangedSignature(Entity entity, Signature signature)
        {
            for (auto const& pair : m_Systems)
            {
                UpdateMembership(*pair.second, m_Signatures[pair.first], entity, signature);
            }
        }

    private:
        static void UpdateMembership(System& system, Signature systemSignature, Entity entity, Signature signature)
        {
            auto it = std::find(system.m_Entities.begin(), system.m_Entities.end(), entity);
            if ((systemSignature & signature) == systemSignature)
            {
                if (it == system.m_Entities.end())
                {
                    system.m_Entities.push_back(entity);
                }
            }
            else if (it != system.m_Entities.end())
            {
                system.m_Entities.erase(it);
            }
        }

        std::unordered_map<std::type_index, Signature>              m_Signatures{};
        std::unordered_map<std::type_index, std::shared_ptr<System>> m_Systems{};
        std::vector<std::shared_ptr<System>>                         m_Order{};
        // The systems of each phase, in registration order.
        std::array<std::vector<std::shared_ptr<System>>, SYSTEM_PHASE_COUNT> m_Phases{};
    };
}
