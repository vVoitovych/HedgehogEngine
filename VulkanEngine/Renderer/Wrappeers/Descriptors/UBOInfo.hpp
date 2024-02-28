#pragma once

#include "Renderer/Containers/Light.hpp"
#include "Renderer/Common/RendererSettings.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE 
#include <glm/glm.hpp>

namespace Renderer
{
    struct UniformBufferObject 
    {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        Light lights[MAX_LIGHTS_COUNT];
        int lightCount;
    };
}

