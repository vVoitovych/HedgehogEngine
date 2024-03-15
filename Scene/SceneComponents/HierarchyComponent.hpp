#pragma once

#include "ECS/Entity.h"
#include <vector>
#include <string>

namespace Scene
{
	class HierarchyComponent
	{
	public:
		std::string mName;

		ECS::Entity mParent = 0;
		std::vector<ECS::Entity> mChildren;

	};

}




