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

            m_Signatures.insert({ typeId, signature });
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
                auto const& typeId          = pair.first;
                auto const& system          = pair.second;
                auto const& systemSignature = m_Signatures[typeId];

                if ((systemSignature & signature) == systemSignature)
                {
                    auto it = std::find(system->m_Entities.begin(), system->m_Entities.end(), entity);
                    if (it == system->m_Entities.end())
                    {
                        system->m_Entities.push_back(entity);
                    }
                }
                else
                {
                    auto it = std::find(system->m_Entities.begin(), system->m_Entities.end(), entity);
                    if (it != system->m_Entities.end())
                    {
                        system->m_Entities.erase(it);
                    }
                }
            }
        }

    private:
        std::unordered_map<std::type_index, Signature>              m_Signatures{};
        std::unordered_map<std::type_index, std::shared_ptr<System>> m_Systems{};
    };
}
