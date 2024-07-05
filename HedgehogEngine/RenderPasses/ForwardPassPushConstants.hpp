#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

namespace Renderer
{
	struct ForwardPassPushConstants
	{
		glm::mat4 objToWorld;
	};

}





