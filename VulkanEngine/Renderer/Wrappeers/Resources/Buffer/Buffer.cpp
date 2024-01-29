#include "Buffer.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Commands/CommandPool.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"

#include <stdexcept>

namespace Renderer
{
	Buffer::Buffer(
		const std::unique_ptr<Device>& device, 
		VkDeviceSize size, 
		VkBufferUsageFlags usage, 
		VkMemoryPropertyFlags properties)
		: mBuffer(nullptr)
		, mBufferMemory(nullptr)
		, mDevice(nullptr)
	{
		mDevice = device->GetNativeDevice();
		VkBufferCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		info.size = size;
		info.usage = usage;
		info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(mDevice, &info, nullptr, &mBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create buffer");
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(mDevice, mBuffer, &memRequirements);

		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = memRequirements.size;
		allocateInfo.memoryTypeIndex = device->FindMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(mDevice, &allocateInfo, nullptr, &mBufferMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate memory!");
		}

		vkBindBufferMemory(mDevice, mBuffer, mBufferMemory, 0);
	}

	Buffer::~Buffer()
	{
		if (mBuffer != nullptr)
		{
			LOGERROR("Vulkan buffer should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mBufferMemory != nullptr)
		{
			LOGERROR("Vulkan buffer memory should be cleanedup before destruction!");
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

	void Buffer::CopyDataToBufferMemory(const void* data, size_t size)
	{
		void* tempData;
		vkMapMemory(mDevice, mBufferMemory, 0, size, 0, &tempData);
		memcpy(tempData, data, size);
		vkUnmapMemory(mDevice, mBufferMemory);
	}

	void Buffer::MapMemory(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData)
	{
		vkMapMemory(mDevice, mBufferMemory, offset, size, flags, ppData);
	}

	void Buffer::DestroyBuffer()
	{
		vkDestroyBuffer(mDevice, mBuffer, nullptr);
		vkFreeMemory(mDevice, mBufferMemory, nullptr);
		mBufferMemory = nullptr;
		mBuffer = nullptr;
	}

	const VkBuffer& Buffer::GetNativeBuffer() const
	{
		return mBuffer;
	}

}

