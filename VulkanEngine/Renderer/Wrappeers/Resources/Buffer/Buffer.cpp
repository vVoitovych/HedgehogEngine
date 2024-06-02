#include "Buffer.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"

#include "ThirdParty/VulkanMemoryAllocator/vk_mem_alloc.h"

#include <stdexcept>

namespace Renderer
{
	Buffer::Buffer(
		const Device& device, 
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

		if (vmaCreateBuffer(device.GetAllocator(), &bufferInfo, &vmaallocInfo,	&mBuffer, &mAllocation, nullptr) != VK_SUCCESS)
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

	void Buffer::CopyDataToBufferMemory(const Device& device, const void* data, size_t size)
	{
		void* tempData;
		MapMemory(device, &tempData);
		memcpy(tempData, data, size);
		UnapMemory(device);
	}

	void Buffer::DestroyBuffer(const Device& device)
	{
		vmaDestroyBuffer(device.GetAllocator(), mBuffer, mAllocation);
		mAllocation = nullptr;
		mBuffer = nullptr;
	}

	void Buffer::MapMemory(const Device& device, void** ppData)
	{
		vmaMapMemory(device.GetAllocator(), mAllocation, ppData);
	}

	void Buffer::UnapMemory(const Device& device)
	{
		vmaUnmapMemory(device.GetAllocator(), mAllocation);
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

