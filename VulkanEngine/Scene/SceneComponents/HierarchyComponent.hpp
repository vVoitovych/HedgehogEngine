#pragma once

#include "VulkanEngine/ECS/Entity.h"
#include <vector>
#include <string>

namespace Scene
{
	class HierarchyComponent
	{
	public:
		std::string mName;

		ECS::Entity mParent;
		std::vector<ECS::Entity> mChildren;

	};

}




