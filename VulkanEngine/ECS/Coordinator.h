#pragma once

#include <memory>

#include "ComponentManager.h"
#include "EntityManager.h"
#include "SystemManager.h"

namespace ECS
{
	class Coordinator
	{
	public:
		void Init()
		{
			componentManager = std::make_unique<ComponentManager>();
			entityManager = std::make_unique<EntityManager>();
			systemManager = std::make_unique<SystemManager>();
		}
		// Entity methods
		Entity CreateEntity()
		{
			return  entityManager->CreateEntity();
		}

		void CreateEntity(Entity entity)
		{
			entityManager->CreateEntity(entity);
		}

		void DestroyEntity(Entity entity)
		{
			entityManager->DestroyEntity(entity);
			componentManager->EntityDestroyed(entity);
			systemManager->EntityDestroyed(entity);
		}
		// Component methods
		template<typename  T>
		void RegisterComponent()
		{
			componentManager->RegisterComponent<T>();
		}

		template<typename T>
		void AddComponent(Entity entity, T component)
		{
			componentManager->AddComponent<T>(entity, component);

			auto signature = entityManager->GetSignature(entity);
			signature.set(componentManager->GetComponentType<T>(), true);
			entityManager->SetSignature(entity, signature);

			systemManager->EntityChangedSignature(entity, signature);
		}


		template<typename T>
		void RemoveComponent(Entity entity)
		{
			componentManager->RemoveComponent<T>(entity);

			auto signature = entityManager->GetSignature(entity);
			signature.set(componentManager->GetComponentType<T>(), false);
			entityManager->SetSignature(entity, signature);

			systemManager->EntityChangedSignature(entity, signature);
		}

		template<typename T>
		T& GetComponent(Entity entity) const
		{
			return componentManager->GetComponent<T>(entity);
		}

		template<typename T>
		bool HasComponent(Entity entity) const
		{
			return componentManager->HasComponent<T>(entity);
		}

		template<typename T>
		ComponentType GetComponentType() const
		{
			return componentManager->GetComponentType<T>();
		}

		// System methods
		template<typename T>
		std::shared_ptr<T> RegisterSystem()
		{
			return systemManager->RegisterSystem<T>();
		}

		template<typename T>
		void SetSystemSignature(Signature signature)
		{
			systemManager->SetSignature<T>(signature);
		}

	private:
		std::unique_ptr<ComponentManager> componentManager;
		std::unique_ptr<EntityManager> entityManager;
		std::unique_ptr<SystemManager> systemManager;
	};
}
