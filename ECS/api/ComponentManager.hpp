#pragma once

#include "Entity.hpp"
#include "ComponentArray.hpp"

#include <unordered_map>
#include <memory>
#include <cassert>
#include <typeindex>

namespace ECS
{
    class ComponentManager
    {
    public:
        template<typename T>
        void RegisterComponent()
        {
            const std::type_index typeId = typeid(T);
            assert(m_ComponentTypes.find(typeId) == m_ComponentTypes.end() &&
                "Component already registered!");

            m_ComponentTypes.insert({ typeId, m_NextComponentType });
            m_ComponentArrays.insert({ typeId, std::make_shared<ComponentArray<T>>() });
            ++m_NextComponentType;
        }

        template<typename T>
        ComponentType GetComponentType() const
        {
            const std::type_index typeId = typeid(T);
            assert(m_ComponentTypes.find(typeId) != m_ComponentTypes.end() &&
                "Component not registered!");

            return m_ComponentTypes.at(typeId);
        }

        template<typename T>
        void AddComponent(Entity entity, T component)
        {
            GetComponentArray<T>()->InsertData(entity, component);
        }

        template<typename T>
        void RemoveComponent(Entity entity)
        {
            GetComponentArray<T>()->RemoveData(entity);
        }

        template<typename T>
        T& GetComponent(Entity entity) const
        {
            return GetComponentArray<T>()->GetData(entity);
        }

        template<typename T>
        bool HasComponent(Entity entity) const
        {
            return GetComponentArray<T>()->HasData(entity);
        }

        void EntityDestroyed(Entity entity)
        {
            for (auto const& pair : m_ComponentArrays)
            {
                pair.second->EntityDestroyed(entity);
            }
        }

    private:
        std::unordered_map<std::type_index, ComponentType>                    m_ComponentTypes{};
        std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>> m_ComponentArrays{};

        ComponentType m_NextComponentType{};

        template<typename T>
        std::shared_ptr<ComponentArray<T>> GetComponentArray() const
        {
            const std::type_index typeId = typeid(T);
            assert(m_ComponentArrays.find(typeId) != m_ComponentArrays.end() &&
                "Component not registered!");

            return std::static_pointer_cast<ComponentArray<T>>(m_ComponentArrays.at(typeId));
        }
    };
}
