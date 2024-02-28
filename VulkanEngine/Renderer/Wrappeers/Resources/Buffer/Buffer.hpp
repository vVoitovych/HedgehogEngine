#pragma once

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
			VkMemoryPropertyFlags properties
		);
		~Buffer();

		void CopyBuffer(VkBuffer dstBuffer, VkDeviceSize size, const std::unique_ptr<CommandPool>& commandPool);
		void CopyBufferToImage(VkImage image, uint32_t width, uint32_t height, const std::unique_ptr<CommandPool>& commandPool);

		void CopyDataToBufferMemory(const void* data, size_t size);
		void MapMemory(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData);
		void DestroyBuffer();

		const VkBuffer& GetNativeBuffer() const;
		VkDeviceSize GetBufferSize() const;

	private:
		VkDeviceSize mBufferSize;
		VkBuffer mBuffer;
		VkDeviceMemory mBufferMemory;

		VkDevice mDevice;
	};

}





