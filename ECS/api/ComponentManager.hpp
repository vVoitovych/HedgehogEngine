#pragma once

#include "Entity.hpp"
#include "ComponentArray.hpp"

#include <algorithm>
#include <unordered_map>
#include <memory>
#include <cassert>
#include <typeindex>
#include <utility>
#include <vector>

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

            // The lowest id an unregistered type freed, else the next new one.
            ComponentType type = m_NextComponentType;
            if (!m_FreeComponentTypes.empty())
            {
                const auto lowest = std::min_element(m_FreeComponentTypes.begin(), m_FreeComponentTypes.end());
                type              = *lowest;
                m_FreeComponentTypes.erase(lowest);
            }
            else
            {
                assert(m_NextComponentType < MAX_COMPONENTS && "Too many component types.");
                ++m_NextComponentType;
            }
            m_ComponentTypes.insert({ typeId, type });
            m_ComponentArrays.insert({ typeId, std::make_shared<ComponentArray<T>>() });
        }

        // Drops T's storage and frees its id for the next registration. The caller removes every
        // T first, so no entity's signature still carries the id.
        template<typename T>
        void UnregisterComponent()
        {
            const std::type_index typeId = typeid(T);
            const auto            found  = m_ComponentTypes.find(typeId);
            assert(found != m_ComponentTypes.end() && "Component not registered!");
            m_FreeComponentTypes.push_back(found->second);
            m_ComponentTypes.erase(found);
            m_ComponentArrays.erase(typeId);
        }

        template<typename T>
        bool IsRegistered() const
        {
            return m_ComponentTypes.find(typeid(T)) != m_ComponentTypes.end();
        }

        // Every entity holding a T.
        template<typename T>
        std::vector<Entity> GetEntitiesWith() const
        {
            return GetComponentArray<T>()->GetEntities();
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
            GetComponentArray<T>()->NotifyAndRemoveData(entity);
        }

        template<typename T>
        void SetComponentRemovedCallback(typename ComponentArray<T>::RemovedCallback callback)
        {
            GetComponentArray<T>()->SetRemovedCallback(std::move(callback));
        }

        template<typename T>
        typename ComponentArray<T>::RemovedCallback GetComponentRemovedCallback() const
        {
            return GetComponentArray<T>()->GetRemovedCallback();
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

        // Every removal callback of the entity runs before any of its components is removed,
        // so a callback can still read the entity's other components.
        void EntityDestroyed(Entity entity)
        {
            for (auto const& pair : m_ComponentArrays)
            {
                pair.second->NotifyEntityDestroyed(entity);
            }
            for (auto const& pair : m_ComponentArrays)
            {
                pair.second->EntityDestroyed(entity);
            }
        }

    private:
        std::unordered_map<std::type_index, ComponentType>                    m_ComponentTypes{};
        std::unordered_map<std::type_index, std::shared_ptr<IComponentArray>> m_ComponentArrays{};

        ComponentType              m_NextComponentType{};
        std::vector<ComponentType> m_FreeComponentTypes{};

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
