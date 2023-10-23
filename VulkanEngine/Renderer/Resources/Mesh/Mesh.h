#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "Vertex.h"

namespace Renderer
{
	class Device;

	class Mesh
	{
	public:
		Mesh();
		~Mesh();

		void Initialize(const Device& device);
		void Cleanup(const Device& device);

		VkBuffer GetVertexBuffer();
		VkBuffer GetIndexBuffer();

		uint32_t GetIndiciesCount();

	private:
		void CreateVertexBuffer(const Device& device);
		void CreateIndexBuffer(const Device& device);

	private:
		std::vector<Vertex> mVerticies;
		std::vector<uint16_t>mIndicies;

		VkBuffer mVertexBuffer;
		VkDeviceMemory mVertexBufferMemory;

		VkBuffer mIndexBuffer;
		VkDeviceMemory mIndexBufferMemory;
	};

}



