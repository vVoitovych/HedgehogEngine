#pragma once

#include "Entity.hpp"

#include <unordered_map>
#include <array>
#include <cassert>

namespace ECS
{
    class IComponentArray
    {
    public:
        virtual ~IComponentArray() = default;
        virtual void EntityDestroyed(Entity entity) = 0;
    };

    template<typename T>
    class ComponentArray : public IComponentArray
    {
    public:
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
    };
}
