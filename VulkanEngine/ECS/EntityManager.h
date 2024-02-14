#pragma once

#include <vector>
#include <array>
#include "Entity.h"

namespace ECS
{
	class EntityManager
	{
	public:
		EntityManager();

		Entity CreateEntity();
		void CreateEntity(Entity entity);
		void DestroyEntity(Entity entity);

		Signature GetSignature(Entity entity) const;
		void SetSignature(Entity entity, Signature signature);

	private:
		std::vector<Entity> entityPool;
		std::array<Signature, MAX_ENTITIES> signatures;

		size_t entityCount;
	};
}

