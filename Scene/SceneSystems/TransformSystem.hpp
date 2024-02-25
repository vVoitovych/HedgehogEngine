#pragma once

#include "ECS/System.h"
#include "ECS/Coordinator.h"

namespace Scene
{
	class TransformSystem : public ECS::System 
	{
	public:
		void Update(ECS::Coordinator& coordinator);
	};
}


