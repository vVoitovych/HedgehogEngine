#pragma once
#include "Entity.h"

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
			assert(entityToIndexMap.find(entity) == entityToIndexMap.end() && "Try to add component to same entity twice.");

			size_t index = size;
			entityToIndexMap[entity] = index;
			indexToEntityMap[index] = entity;
			componentsArray[index] = component;
			++size;
		}

		void RemoveData(Entity entity)
		{
			assert(entityToIndexMap.find(entity) != entityToIndexMap.end() && "Try to delete unexisting component.");

			size_t indexOfRemovedEntity = entityToIndexMap[entity];
			size_t indexOfLastElement = size - 1;
			componentsArray[indexOfRemovedEntity] = componentsArray[indexOfLastElement];

			const Entity entityOfLastElement = indexToEntityMap[indexOfLastElement];
			entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
			indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

			entityToIndexMap.erase(entity);
			indexToEntityMap.erase(indexOfLastElement);

			--size;
		}

		T& GetData(Entity entity)
		{
			assert(entityToIndexMap.find(entity) != entityToIndexMap.end() && "Entity doesn't have component.");

			return componentsArray[entityToIndexMap[entity]];
		}

		bool HasData(Entity entity)
		{
			return entityToIndexMap.find(entity) != entityToIndexMap.end();
		}

		void EntityDestroyed(Entity entity) override
		{
			if (entityToIndexMap.find(entity) != entityToIndexMap.end())
			{
				RemoveData(entity);
			}
		}

	private:
		std::array<T, MAX_ENTITIES> componentsArray;
		std::unordered_map<Entity, size_t> entityToIndexMap;
		std::unordered_map<size_t, Entity> indexToEntityMap;

		size_t size = 0;
	};
}






