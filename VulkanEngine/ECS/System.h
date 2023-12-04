#pragma once
#include "Entity.h"

#include <set>

namespace ECS
{
	class System
	{
	public:
		std::set<Entity> entities;
	};
}
