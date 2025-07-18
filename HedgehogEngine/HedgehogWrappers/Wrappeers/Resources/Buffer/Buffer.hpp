#pragma once

#include "vma/vk_mem_alloc.h"

#include <vulkan/vulkan.h>

namespace Wrappers
{
	class Device;
	class CommandPool;
	
	class Buffer
	{
	public:
		Buffer(
			const Device& device,
			VkDeviceSize size, 
			VkBufferUsageFlags usage, 
			VmaMemoryUsage memUsage
		);
		~Buffer();

		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;

		Buffer(Buffer&& other) noexcept;
		Buffer& operator=(Buffer&& other) noexcept;

		void CopyDataToBufferMemory(const Device& device, const void* data, size_t size);
		void DestroyBuffer(const Device& device);

		void MapMemory(const Device& device, void** ppData);
		void UnapMemory(const Device& device);

		const VkBuffer& GetNativeBuffer() const;
		VkDeviceSize GetBufferSize() const;

	private:
		VkDeviceSize m_BufferSize;
		VkBuffer m_Buffer;
		VmaAllocation m_Allocation;

	};

}





