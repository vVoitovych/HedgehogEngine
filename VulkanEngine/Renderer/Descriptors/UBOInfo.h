#pragma once
#include "VulkanEngine/Renderer/Common/pch.h"

namespace Renderer
{
    struct UniformBufferObject 
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };
}

