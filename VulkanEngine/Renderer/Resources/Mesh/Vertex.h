#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace Renderer
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 texCoord;

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 3>  GetAttributeDescription();

		static std::vector<Vertex> GetSimpleTriangle();
		static std::vector<Vertex> GetQuad();
		static std::vector<uint16_t> GetQuadIndecies();

	};


}


