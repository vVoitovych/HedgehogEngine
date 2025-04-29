#include "Buffer.hpp"

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogCommon/Common/EngineDebugBreak.hpp"

#include "Logger/Logger.hpp"

#include "vma/vk_mem_alloc.h"

#include <stdexcept>

namespace Wrappers
{
	Buffer::Buffer(
		const Device& device, 
		VkDeviceSize size, 
		VkBufferUsageFlags usage, 
		VmaMemoryUsage memUsage)
		: m_Buffer(nullptr)
		, m_Allocation(nullptr)
	{
		m_BufferSize = size;

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo vmaallocInfo = {};
		vmaallocInfo.usage = memUsage; //VMA_MEMORY_USAGE_CPU_TO_GPU;

		if (vmaCreateBuffer(device.GetAllocator(), &bufferInfo, &vmaallocInfo,	&m_Buffer, &m_Allocation, nullptr) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create buffer");
		}

	}

	Buffer::~Buffer()
	{
		if (m_Buffer != nullptr)
		{
			LOGERROR("Vulkan buffer should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (m_Allocation != nullptr)
		{
			LOGERROR("Vulkan buffer allocation should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	Buffer::Buffer(Buffer&& other) noexcept
		: m_Buffer(other.m_Buffer)
		, m_Allocation(other.m_Allocation)
		, m_BufferSize(other.m_BufferSize)
	{
		other.m_Buffer = nullptr;
		other.m_Allocation = nullptr;
	}

	Buffer& Buffer::operator=(Buffer&& other) noexcept
	{
		if (this != &other)
		{
			m_Buffer = other.m_Buffer;
			m_Allocation = other.m_Allocation;
			m_BufferSize = other.m_BufferSize;

			other.m_Buffer = nullptr;
			other.m_Allocation = nullptr;
		}
		return *this;
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
		vmaDestroyBuffer(device.GetAllocator(), m_Buffer, m_Allocation);
		m_Allocation = nullptr;
		m_Buffer = nullptr;
	}

	void Buffer::MapMemory(const Device& device, void** ppData)
	{
		vmaMapMemory(device.GetAllocator(), m_Allocation, ppData);
	}

	void Buffer::UnapMemory(const Device& device)
	{
		vmaUnmapMemory(device.GetAllocator(), m_Allocation);
	}

	const VkBuffer& Buffer::GetNativeBuffer() const
	{
		return m_Buffer;
	}

	VkDeviceSize Buffer::GetBufferSize() const
	{
		return m_BufferSize;
	}

}

