#pragma once

#include "ECS/System.h"
#include "ECS/ECS.h"

namespace Scene
{
	class TransformSystem : public ECS::System
	{
	public:
		void Update(ECS::ECS& ecs);
	};
}


