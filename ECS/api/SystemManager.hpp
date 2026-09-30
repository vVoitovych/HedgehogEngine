#pragma once

#include "Entity.hpp"
#include "System.hpp"

#include <unordered_map>
#include <memory>
#include <cassert>
#include <algorithm>
#include <typeindex>

namespace ECS
{
    class SystemManager
    {
    public:
        template<typename T>
        std::shared_ptr<T> RegisterSystem()
        {
            const std::type_index typeId = typeid(T);
            assert(m_Systems.find(typeId) == m_Systems.end() &&
                "System already registered!");

            auto system = std::make_shared<T>();
            m_Systems.insert({ typeId, system });
            return system;
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
    };
}
