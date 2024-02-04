#pragma once

#include "Entity.h"
#include "ComponentArray.h"

#include <unordered_map>
#include <memory>
#include <cassert>

namespace ECS
{
	using ComponentType = size_t;

	class ComponentManager
	{
	public:
		template<typename T>
		void RegisterComponent()
		{
			const char* typeName = typeid(T).name();
			assert(componentTypes.find(typeName) == componentTypes.end() && "Component alredy registered!");

			componentTypes.insert({ typeName, nextComponentType });
			componentArrays.insert({ typeName, std::make_shared<ComponentArray<T>>() });
			++nextComponentType;
		}

		template<typename T>
		ComponentType GetComponentType()
		{
			const char* typeName = typeid(T).name();
			assert(componentTypes.find(typeName) != componentTypes.end() && "Component not registered!");

			return  componentTypes[typeName];
		}

		template<typename T>
		void AddComponent(Entity entity, T component)
		{
			GetComponentArray<T>()->InsertData(entity, component);
		}

		template<typename T>
		void RemoveComponent(Entity entity)
		{
			GetComponentArray<T>()->RemoveData(entity);
		}

		template<typename T>
		T& GetComponent(Entity entity)
		{
			return GetComponentArray<T>()->GetData(entity);
		}

		template<typename T>
		bool HasComponent(Entity entity)
		{
			return GetComponentArray<T>()->HasData(entity);
		}

		void EntityDestroyed(Entity entity)
		{
			for (auto const& pair : componentArrays)
			{
				auto const& component = pair.second;
				component->EntityDestroyed(entity);
			}
		}

	private:
		std::unordered_map<const char*, ComponentType> componentTypes{};
		std::unordered_map<const char*, std::shared_ptr<IComponentArray>> componentArrays{};

		ComponentType nextComponentType{};

		template<typename T>
		std::shared_ptr<ComponentArray<T>> GetComponentArray()
		{
			const char* typeName = typeid(T).name();
			assert(componentArrays.find(typeName) != componentArrays.end() && "Component not registered!");

			return std::static_pointer_cast<ComponentArray<T>>(componentArrays[typeName]);
		}
	};
}


