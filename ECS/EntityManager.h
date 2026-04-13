#pragma once

#include <vector>
#include <array>
#include "Entity.h"
#include "EcsApi.hpp"

namespace ECS
{
#pragma warning(push)
#pragma warning(disable: 4251) // private STL members are safe across DLL boundary when using the same runtime
	class ECS_API EntityManager
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
#pragma warning(pop)
}

