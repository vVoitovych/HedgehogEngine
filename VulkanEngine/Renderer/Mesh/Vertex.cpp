#include "Vertex.h"

namespace Renderer
{
	VkVertexInputBindingDescription Vertex::GetBindingDescription()
	{
		VkVertexInputBindingDescription desc{};

		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return desc;
	}

	std::array<VkVertexInputAttributeDescription, 2> Vertex::GetAttributeDescription()
	{
		std::array<VkVertexInputAttributeDescription, 2> desc;

		desc[0].binding = 0;
		desc[0].location = 0;
		desc[0].format = VK_FORMAT_R32G32_SFLOAT;
		desc[0].offset = offsetof(Vertex, pos);

		desc[1].binding = 0;
		desc[1].location = 1;
		desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		desc[1].offset = offsetof(Vertex, color);

		return desc;
	}


	std::vector<Vertex> Vertex::GetSimpleTriangle()
	{
		std::vector<Vertex> vertices = 
		{
			{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
			{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
		};

		return vertices;
	}
	std::vector<Vertex> Vertex::GetQuad()
	{
		std::vector<Vertex> vertices = 
		{
			{{-0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
			{{0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}},
			{{-0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}
		};

		return vertices;
	}
	std::vector<uint16_t> Vertex::GetQuadIndecies()
	{
		std::vector<uint16_t> indices =	{ 0, 1, 2, 2, 3, 0 };

		return indices;
	}
}


