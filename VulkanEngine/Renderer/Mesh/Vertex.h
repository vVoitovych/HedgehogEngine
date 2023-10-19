#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

namespace Renderer
{
	struct Vertex
	{
		glm::vec2 pos;
		glm::vec3 color;
		glm::vec2 texCoord;

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 3>  GetAttributeDescription();

		static std::vector<Vertex> GetSimpleTriangle();
		static std::vector<Vertex> GetQuad();
		static std::vector<uint16_t> GetQuadIndecies();

	};


}


