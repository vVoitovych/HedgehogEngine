#pragma once

#include "..\Common\pch.h"
#include "Vertex.h"

namespace Renderer
{
	class Device;
	class CommandPool;

	class Mesh
	{
	public:
		Mesh();
		~Mesh();

		void Initialize(Device& device, CommandPool& commandPool);
		void Cleanup(Device& device);

		VkBuffer GetVertexBuffer();
		VkBuffer GetIndexBuffer();

		uint32_t GetIndiciesCount();

	private:
		void CreateVertexBuffer(Device& device, CommandPool& commandPool);
		void CreateIndexBuffer(Device& device, CommandPool& commandPool);

	private:
		std::vector<Vertex> mVerticies;
		std::vector<uint16_t>mIndicies;

		VkBuffer mVertexBuffer;
		VkDeviceMemory mVertexBufferMemory;

		VkBuffer mIndexBuffer;
		VkDeviceMemory mIndexBufferMemory;
	};

}



