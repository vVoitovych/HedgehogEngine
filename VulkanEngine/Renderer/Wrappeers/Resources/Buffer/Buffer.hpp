#pragma once

#include "ThirdParty/VulkanMemoryAllocator/vk_mem_alloc.h"

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;
	class CommandPool;
	
	class Buffer
	{
	public:
		Buffer(
			const std::unique_ptr<Device>& device,
			VkDeviceSize size, 
			VkBufferUsageFlags usage, 
			VmaMemoryUsage memUsage
		);
		~Buffer();

		void CopyBuffer(VkBuffer dstBuffer, VkDeviceSize size, const std::unique_ptr<CommandPool>& commandPool);
		void CopyBufferToImage(VkImage image, uint32_t width, uint32_t height, const std::unique_ptr<CommandPool>& commandPool);

		void CopyDataToBufferMemory(const std::unique_ptr<Device>& device, const void* data, size_t size);
		void DestroyBuffer(const std::unique_ptr<Device>& device);

		void MapMemory(const std::unique_ptr<Device>& device, void** ppData);
		void UnapMemory(const std::unique_ptr<Device>& device);

		const VkBuffer& GetNativeBuffer() const;
		VkDeviceSize GetBufferSize() const;

	private:
		VkDeviceSize mBufferSize;
		VkBuffer mBuffer;
		VmaAllocation mAllocation;

	};

}





