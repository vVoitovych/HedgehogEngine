#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	void CreateBuffer(
		VkDevice device, 
		VkPhysicalDevice physicalDevice, 
		VkDeviceSize size, 
		VkBufferUsageFlags usage, 
		VkMemoryPropertyFlags properties, 
		VkBuffer& buffer, 
		VkDeviceMemory& bufferMemory);

	void CopyBuffer(
		VkDevice device, 
		VkCommandPool commandPool,
		VkQueue queue,
		VkBuffer srcBuffer, 
		VkBuffer dstBuffer, 
		VkDeviceSize size);

	uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
}

