#pragma once

#include "VulkanEngine/ECS/System.h"
#include "VulkanEngine/ECS/Coordinator.h"

namespace Scene
{
	class TransformSystem : public ECS::System 
	{
	public:
		void Update(ECS::Coordinator& coordinator);
	};
}


