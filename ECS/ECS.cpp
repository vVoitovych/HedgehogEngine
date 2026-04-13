#include "ECS.h"

namespace ECS
{
	void ECS::Init()
	{
		componentManager = std::make_unique<ComponentManager>();
		entityManager = std::make_unique<EntityManager>();
		systemManager = std::make_unique<SystemManager>();
	}

	Entity ECS::CreateEntity()
	{
		return entityManager->CreateEntity();
	}

	void ECS::CreateEntity(Entity entity)
	{
		entityManager->CreateEntity(entity);
	}

	void ECS::DestroyEntity(Entity entity)
	{
		entityManager->DestroyEntity(entity);
		componentManager->EntityDestroyed(entity);
		systemManager->EntityDestroyed(entity);
	}
}
