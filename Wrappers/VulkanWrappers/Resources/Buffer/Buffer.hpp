#pragma once

#include "ThirdParty/VulkanMemoryAllocator/vk_mem_alloc.h"

#include <vulkan/vulkan.h>

namespace Hedgehog
{
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

			void CopyDataToBufferMemory(const Device& device, const void* data, size_t size);
			void DestroyBuffer(const Device& device);

			void MapMemory(const Device& device, void** ppData);
			void UnapMemory(const Device& device);

			const VkBuffer& GetNativeBuffer() const;
			VkDeviceSize GetBufferSize() const;

		private:
			VkDeviceSize mBufferSize;
			VkBuffer mBuffer;
			VmaAllocation mAllocation;

		};
	}
}





