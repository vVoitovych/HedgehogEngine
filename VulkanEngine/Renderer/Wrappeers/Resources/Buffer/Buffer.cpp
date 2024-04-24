#include "Buffer.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Commands/CommandPool.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"

#include "ThirdParty/VulkanMemoryAllocator/vk_mem_alloc.h"

#include <stdexcept>

namespace Renderer
{
	Buffer::Buffer(
		const std::unique_ptr<Device>& device, 
		VkDeviceSize size, 
		VkBufferUsageFlags usage, 
		VmaMemoryUsage memUsage)
		: mBuffer(nullptr)
		, mAllocation(nullptr)
	{
		mBufferSize = size;

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo vmaallocInfo = {};
		vmaallocInfo.usage = memUsage; //VMA_MEMORY_USAGE_CPU_TO_GPU;

		if (vmaCreateBuffer(device->GetAllocator(), &bufferInfo, &vmaallocInfo,	&mBuffer, &mAllocation, nullptr) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create buffer");
		}

	}

	Buffer::~Buffer()
	{
		if (mBuffer != nullptr)
		{
			LOGERROR("Vulkan buffer should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mAllocation != nullptr)
		{
			LOGERROR("Vulkan buffer allocation should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}


	void Buffer::CopyBuffer(VkBuffer dstBuffer, VkDeviceSize size, const std::unique_ptr<CommandPool>& commandPool)
	{
		VkCommandBuffer commandBuffer = commandPool->BeginSingleTimeCommands();

		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(commandBuffer, mBuffer, dstBuffer, 1, &copyRegion);

		commandPool->EndSingleTimeCommands(commandBuffer);
	}

	void Buffer::CopyBufferToImage(VkImage image, uint32_t width, uint32_t height, const std::unique_ptr<CommandPool>& commandPool)
	{
		VkCommandBuffer commandBuffer = commandPool->BeginSingleTimeCommands();

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(commandBuffer, mBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		commandPool->EndSingleTimeCommands(commandBuffer);
	}

	void Buffer::CopyDataToBufferMemory(const std::unique_ptr<Device>& device, const void* data, size_t size)
	{
		void* tempData;
		MapMemory(device, &tempData);
		memcpy(tempData, data, size);
		UnapMemory(device);
	}

	void Buffer::DestroyBuffer(const std::unique_ptr<Device>& device)
	{
		vmaDestroyBuffer(device->GetAllocator(), mBuffer, mAllocation);
		mAllocation = nullptr;
		mBuffer = nullptr;
	}

	void Buffer::MapMemory(const std::unique_ptr<Device>& device, void** ppData)
	{
		vmaMapMemory(device->GetAllocator(), mAllocation, ppData);
	}

	void Buffer::UnapMemory(const std::unique_ptr<Device>& device)
	{
		vmaUnmapMemory(device->GetAllocator(), mAllocation);
	}

	const VkBuffer& Buffer::GetNativeBuffer() const
	{
		return mBuffer;
	}

	VkDeviceSize Buffer::GetBufferSize() const
	{
		return mBufferSize;
	}

}

