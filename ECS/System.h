#pragma once
#include "Entity.h"

#include <vector>

namespace ECS
{
	class System
	{
	public:
		std::vector<Entity> entities;
	};
}
