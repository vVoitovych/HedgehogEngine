#include "EntityManager.h"
#include <cassert>
#include <algorithm>

namespace ECS
{
	EntityManager::EntityManager()
		: entityCount(0)
	{
		for (int i = MAX_ENTITIES - 1; i >= 0; --i)
		{
			entityPool.push_back(i);
		}
	}

	Entity EntityManager::CreateEntity()
	{
		assert(entityCount < MAX_ENTITIES && "Too many enteties");

		const Entity result = entityPool.back();
		entityPool.pop_back();
		++entityCount;

		return result;
	}

	void EntityManager::CreateEntity(Entity entity)
	{
		assert(entityCount < MAX_ENTITIES && "Too many enteties");
		auto it = std::find(std::begin(entityPool), std::end(entityPool), entity);
		assert(it != std::end(entityPool) && "Entity already created");
		++entityCount;
		entityPool.erase(it);
	}

	void EntityManager::DestroyEntity(Entity entity)
	{
		assert(entity < MAX_ENTITIES && "Entity out of range");
		signatures[entity].reset();
		entityPool.push_back(entity);
		--entityCount;
	}

	Signature EntityManager::GetSignature(Entity entity) const
	{
		assert(entity < MAX_ENTITIES && "Entity out of range");

		return signatures[entity];
	}

	void EntityManager::SetSignature(Entity entity, Signature signature)
	{
		assert(entity < MAX_ENTITIES && "Entity out of range");

		signatures[entity] = signature;
	}
}


