#include "Vertex.h"

namespace Renderer
{
	bool Vertex::operator==(const Vertex& other) const
	{
		return pos == other.pos && color == other.color && texCoord == other.texCoord;
	}

	VkVertexInputBindingDescription Vertex::GetBindingDescription()
	{
		VkVertexInputBindingDescription desc{};

		desc.binding = 0;
		desc.stride = sizeof(Vertex);
		desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return desc;
	}

	std::array<VkVertexInputAttributeDescription, 3> Vertex::GetAttributeDescription()
	{
		std::array<VkVertexInputAttributeDescription, 3> desc;

		desc[0].binding = 0;
		desc[0].location = 0;
		desc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		desc[0].offset = offsetof(Vertex, pos);

		desc[1].binding = 0;
		desc[1].location = 1;
		desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		desc[1].offset = offsetof(Vertex, color);

		desc[2].binding = 0;
		desc[2].location = 2;
		desc[2].format = VK_FORMAT_R32G32_SFLOAT;
		desc[2].offset = offsetof(Vertex, texCoord);

		return desc;
	}

	std::vector<Vertex> Vertex::GetSimpleTriangle()
	{
		std::vector<Vertex> vertices = 
		{
			{{0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
			{{0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}
		};

		return vertices;
	}

	std::vector<Vertex> Vertex::GetQuad()
	{
		std::vector<Vertex> vertices = 
		{
			{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
			{{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
			{{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}
		};

		return vertices;
	}

	std::vector<uint16_t> Vertex::GetQuadIndecies()
	{
		std::vector<uint32_t> indices =	
		{ 
			0, 1, 2, 2, 3, 0 
		};

		return indices;
	}

	std::vector<Vertex> Vertex::GetCubeVirticies()
	{
		std::vector<Vertex> vertices =
		{
			{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
			{{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
			{{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
			{{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},

			{{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
			{{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
			{{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
			{{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}
		};

		return vertices;
	}

	std::vector<uint16_t> Vertex::GetCubeIndecies()
	{
		std::vector<uint16_t> indices =
		{
			 //Top
			 2, 6, 7,
			 2, 3, 7,

			 //Bottom
			 0, 4, 5,
			 0, 1, 5,

			 //Left
			 0, 2, 6,
			 0, 4, 6,

			 //Right
			 1, 3, 7,
			 1, 5, 7,

			 //Front
			 0, 2, 3,
			 0, 1, 3,

			 //Back
			 4, 6, 7,
			 4, 5, 7
		};

		return indices;
	}
}


