#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <array>
#include <functional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

namespace Renderer
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 texCoord;

		bool operator==(const Vertex& other) const;

		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 3>  GetAttributeDescription();

		static std::vector<Vertex> GetSimpleTriangle();
		static std::vector<Vertex> GetQuad();
		static std::vector<uint32_t> GetQuadIndecies();

		static std::vector<Vertex> GetCubeVirticies();
		static std::vector<uint32_t> GetCubeIndecies();
	};


}

namespace std 
{
	template<> 
	struct hash<Renderer::Vertex> 
	{
		size_t operator()(Renderer::Vertex const& vertex) const 
		{
			return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ (hash<glm::vec2>()(vertex.texCoord) << 1);
		}
	};
}
