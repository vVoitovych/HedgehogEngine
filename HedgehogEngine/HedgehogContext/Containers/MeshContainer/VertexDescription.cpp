#include "VertexDescription.hpp"

namespace Context
{
	bool VertexDescription::operator==(const VertexDescription& other) const
	{
		return pos == other.pos && 
			texCoord == other.texCoord &&
			normal == other.normal;
	}

	VkVertexInputBindingDescription VertexDescription::GetBindingDescription()
	{
		VkVertexInputBindingDescription desc{};

		desc.binding = 0;
		desc.stride = sizeof(VertexDescription);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return desc;
	}

	std::array<VkVertexInputAttributeDescription, 3> VertexDescription::GetAttributeDescription()
	{
		std::array<VkVertexInputAttributeDescription, 3> desc;

		desc[0].binding = 0;
		desc[0].location = 0;
		desc[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		desc[0].offset = offsetof(VertexDescription, pos);

		desc[1].binding = 0;
		desc[1].location = 1;
		desc[1].format = VK_FORMAT_R32G32_SFLOAT;
		desc[1].offset = offsetof(VertexDescription, texCoord);

		desc[2].binding = 0;
		desc[2].location = 2;
		desc[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		desc[2].offset = offsetof(VertexDescription, normal);

		return desc;
	}

}




