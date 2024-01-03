#pragma once
#include <unordered_map>
#include <memory>
#include <cassert>

#include "Entity.h"
#include "System.h"

namespace ECS
{
	class SystemManager
	{
	public:
		template<typename T>
		std::shared_ptr<T> RegisterSystem()
		{
			const char* typeName = typeid(T).name();
			assert(systems.find(typeName) == systems.end() && "Register system that already registered!");

			auto system = std::make_shared<T>();
			systems.insert({ typeName, system });
			return system;
		}

		template<typename T>
		void SetSignature(Signature signature)
		{
			const char* typeName = typeid(T).name();
			assert(systems.find(typeName) != systems.end() && "System used before registered!");

			signatures.insert({ typeName, signature });
		}

		void EntityDestroyed(Entity entity)
		{
			for (auto const& pair : systems)
			{
				auto const& system = pair.second;
				system->entities.erase(entity);
			}
		}

		void EntityChangedSignature(Entity entity, Signature signature)
		{
			for (auto const& pair : systems)
			{
				auto const& typeName = pair.first;
				auto const& system = pair.second;
				auto const& systemSignature = signatures[typeName];

				if ((systemSignature & signature) == systemSignature)
				{
					system->entities.insert(entity);
				}
				else
				{
					system->entities.erase(entity);
				}
			}
		}

	private:
		std::unordered_map<const char*, Signature> signatures{};

		std::unordered_map<const char*, std::shared_ptr<System>> systems{};
	};
}


