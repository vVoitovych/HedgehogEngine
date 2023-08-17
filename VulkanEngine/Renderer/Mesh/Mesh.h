#pragma once

#include "VulkanEngine/Renderer/Common/pch.h"
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
		void Cleanup();

		VkBuffer GetVertexBuffer();
		VkBuffer GetIndexBuffer();

		uint32_t GetIndiciesCount();

	private:
		void CreateVertexBuffer();
		void CreateIndexBuffer();

		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
	private:
		VkDevice mDevice;
		VkPhysicalDevice mPhysicalDevice;
		VkQueue mGraphicsQueue;
		VkCommandPool mCommandPool;

		std::vector<Vertex> mVerticies;
		std::vector<uint16_t>mIndicies;

		VkBuffer mVertexBuffer;
		VkDeviceMemory mVertexBufferMemory;

		VkBuffer mIndexBuffer;
		VkDeviceMemory mIndexBufferMemory;
	};

}



