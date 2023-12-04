#pragma once

#include "VulkanEngine/ECS/System.h"
#include "VulkanEngine/ECS/Coordinator.h"

namespace Scene
{
	class HierarchySystem : public ECS::System
	{
	public:
		void Update(ECS::Coordinator& coordinator);
		void SetRoot(ECS::Entity entity);

	private:
		void UpdateChildrenMatricies(ECS::Coordinator& coordinator, ECS::Entity parent);

	private:
		ECS::Entity mRoot;
	};
}



