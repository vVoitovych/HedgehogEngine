#include "Mesh.h"

#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Renderer/Commands/CommandPool.h"

namespace Renderer
{
	Mesh::Mesh()
	{
		mVerticies = Vertex::GetQuad();
		mIndicies = Vertex::GetQuadIndecies();
	}

	Mesh::~Mesh()
	{
	}

	void Mesh::Initialize(Device& device, CommandPool& commandPool)
	{
		mDevice = device.GetNativeDevice();
		mPhysicalDevice = device.GetNativePhysicalDevice();
		mGraphicsQueue = device.GetNativeGraphicsQueue();
		mCommandPool = commandPool.GetNativeCommandPool();

		CreateVertexBuffer();
		CreateIndexBuffer();
	}

	void Mesh::Cleanup()
	{
		vkDestroyBuffer(mDevice, mVertexBuffer, nullptr);
		vkFreeMemory(mDevice, mVertexBufferMemory, nullptr);
		mVertexBuffer = nullptr;
		mVertexBufferMemory = nullptr;
		std::cout << "Vertex buffer cleaned" << std::endl;

		vkDestroyBuffer(mDevice, mIndexBuffer, nullptr);
		vkFreeMemory(mDevice, mIndexBufferMemory, nullptr);
		mIndexBuffer = nullptr;
		mIndexBufferMemory = nullptr;
		std::cout << "Index buffer cleaned" << std::endl;
	}

	VkBuffer Mesh::GetVertexBuffer()
	{
		return mVertexBuffer;
	}

	VkBuffer Mesh::GetIndexBuffer()
	{
		return mIndexBuffer;
	}

	uint32_t Mesh::GetIndiciesCount()
	{
		return static_cast<uint32_t>(mIndicies.size());
	}

	void Mesh::CreateVertexBuffer()
	{
		VkDeviceSize size = sizeof(mVerticies[0]) * mVerticies.size();

		VkBuffer staginBuffer;
		VkDeviceMemory staginBufferMemory;
		CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staginBuffer, staginBufferMemory);

		void* data;
		vkMapMemory(mDevice, staginBufferMemory, 0, size, 0, &data);
		memcpy(data, mVerticies.data(), (size_t)size);
		vkUnmapMemory(mDevice, staginBufferMemory);

		CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mVertexBuffer, mVertexBufferMemory);
		CopyBuffer(staginBuffer, mVertexBuffer, size);

		vkDestroyBuffer(mDevice, staginBuffer, nullptr);
		vkFreeMemory(mDevice, staginBufferMemory, nullptr);
		std::cout << "Vertex buffer created" << std::endl;
	}

	void Mesh::CreateIndexBuffer()
	{
		VkDeviceSize size = sizeof(mIndicies[0]) * mIndicies.size();

		VkBuffer staginBuffer;
		VkDeviceMemory staginBufferMemory;
		CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			staginBuffer, staginBufferMemory);

		void* data;
		vkMapMemory(mDevice, staginBufferMemory, 0, size, 0, &data);
		memcpy(data, mIndicies.data(), (size_t)size);
		vkUnmapMemory(mDevice, staginBufferMemory);

		CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			mIndexBuffer, mIndexBufferMemory);
		CopyBuffer(staginBuffer, mIndexBuffer, size);

		vkDestroyBuffer(mDevice, staginBuffer, nullptr);
		vkFreeMemory(mDevice, staginBufferMemory, nullptr);
		std::cout << "Index buffer created" << std::endl;
	}

	void Mesh::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{
		VkBufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = size;
		info.usage = usage;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(mDevice, &info, nullptr, &buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create buffer");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(mDevice, buffer, &memRequirements);

		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memRequirements.size;
		allocateInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(mDevice, &allocateInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate memory!");
		}

		vkBindBufferMemory(mDevice, buffer, bufferMemory, 0);
	}

	void Mesh::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = mCommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(mGraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(mGraphicsQueue);

		vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
	}

	uint32_t Mesh::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		throw std::runtime_error("Failed to find memory type");
	}

}


