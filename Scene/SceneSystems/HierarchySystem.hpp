#pragma once

#include "ECS/System.h"
#include "ECS/ECS.h"

namespace Scene
{
	class HierarchySystem : public ECS::System
	{
	public:
		void Update(ECS::ECS& ecs);
		void SetRoot(ECS::Entity entity);

	private:
		void UpdateChildrenMatricies(ECS::ECS& ecs, ECS::Entity parent);

	private:
		ECS::Entity mRoot = 0;
	};
}



