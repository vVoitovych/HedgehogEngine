#pragma once

#include "Entity.hpp"

#include <unordered_map>
#include <array>
#include <cassert>
#include <functional>
#include <utility>

namespace ECS
{
    class IComponentArray
    {
    public:
        virtual ~IComponentArray() = default;
        // Destroying an entity runs in two passes over every array: first each removal
        // callback, while all of the entity's components are still in place, then the removal.
        virtual void NotifyEntityDestroyed(Entity entity) = 0;
        virtual void EntityDestroyed(Entity entity) = 0;
    };

    template<typename T>
    class ComponentArray : public IComponentArray
    {
    public:
        // Called with the component still in place, just before it is removed.
        // The callback must not add or remove a T on the same entity.
        using RemovedCallback = std::function<void(Entity, T&)>;

        void SetRemovedCallback(RemovedCallback callback)
        {
            m_RemovedCallback = std::move(callback);
        }

        void InsertData(Entity entity, T component)
        {
            assert(m_EntityToIndexMap.find(entity) == m_EntityToIndexMap.end() &&
                "Tried to add component to the same entity twice.");

            size_t index = m_Size;
            m_EntityToIndexMap[entity] = index;
            m_IndexToEntityMap[index] = entity;
            m_ComponentsArray[index] = component;
            ++m_Size;
        }

        void RemoveData(Entity entity)
        {
            assert(m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end() &&
                "Tried to remove a non-existing component.");

            size_t indexOfRemovedEntity = m_EntityToIndexMap[entity];
            size_t indexOfLastElement   = m_Size - 1;
            m_ComponentsArray[indexOfRemovedEntity] = m_ComponentsArray[indexOfLastElement];

            const Entity entityOfLastElement = m_IndexToEntityMap[indexOfLastElement];
            m_EntityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
            m_IndexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

            m_EntityToIndexMap.erase(entity);
            m_IndexToEntityMap.erase(indexOfLastElement);

            --m_Size;
        }

        // Runs the removal callback, then removes the data.
        void NotifyAndRemoveData(Entity entity)
        {
            if (m_RemovedCallback)
            {
                m_RemovedCallback(entity, GetData(entity));
            }
            RemoveData(entity);
        }

        T& GetData(Entity entity)
        {
            assert(m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end() &&
                "Entity does not have the requested component.");

            return m_ComponentsArray[m_EntityToIndexMap[entity]];
        }

        bool HasData(Entity entity) const
        {
            return m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end();
        }

        void NotifyEntityDestroyed(Entity entity) override
        {
            if (m_RemovedCallback && m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end())
            {
                m_RemovedCallback(entity, GetData(entity));
            }
        }

        void EntityDestroyed(Entity entity) override
        {
            if (m_EntityToIndexMap.find(entity) != m_EntityToIndexMap.end())
            {
                RemoveData(entity);
            }
        }

    private:
        std::array<T, MAX_ENTITIES>        m_ComponentsArray{};
        std::unordered_map<Entity, size_t> m_EntityToIndexMap{};
        std::unordered_map<size_t, Entity> m_IndexToEntityMap{};

        size_t m_Size = 0;

        RemovedCallback m_RemovedCallback{};
    };
}
