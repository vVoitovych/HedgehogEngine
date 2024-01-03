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
	class VertexDescription
	{
	public:
		bool operator==(const VertexDescription& other) const;

	public:
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 texCoord;
		glm::vec3 normal;

	public:
		static VkVertexInputBindingDescription GetBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 4>  GetAttributeDescription();

	};
}

namespace std
{
	template<>
	struct hash<Renderer::VertexDescription>
	{
		size_t operator()(Renderer::VertexDescription const& vertex) const
		{
			return (
				(hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^ 
				(hash<glm::vec2>()(vertex.texCoord) << 1) ^
				(hash<glm::vec3>()(vertex.normal));
		}
	};
}


