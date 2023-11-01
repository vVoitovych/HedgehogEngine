#pragma once

#include <queue>
#include <array>
#include "Entity.h"

class EntityManager
{
public:
	EntityManager();

	Entity CreateEntity();
	void DestroyEntity(Entity entity);

	Signature GetSignature(Entity entity) const;
	void SetSignature(Entity entity, Signature signature);

private:
	std::queue<Entity> entityPool;
	std::array<Signature, MAX_ENTITIES> signatures;

	size_t entityCount;
};


