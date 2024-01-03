#include "EntityManager.h"
#include <cassert>

namespace ECS
{
	EntityManager::EntityManager()
		: entityCount(0)
	{
		for (size_t i = 0; i < MAX_ENTITIES; ++i)
		{
			entityPool.push(i);
		}
	}

	Entity EntityManager::CreateEntity()
	{
		assert(entityCount < MAX_ENTITIES && "Too many enteties");

		const Entity result = entityPool.front();
		entityPool.pop();
		++entityCount;

		return result;
	}

	void EntityManager::DestroyEntity(Entity entity)
	{
		assert(entity < MAX_ENTITIES && "Entity out of range");
		signatures[entity].reset();
		entityPool.push(entity);
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


