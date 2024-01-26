#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace Renderer
{
	class Device;

	class CommandPool
	{
	public:
		CommandPool(const std::unique_ptr<Device>& device);
		~CommandPool();

		CommandPool(const CommandPool&) = delete;
		CommandPool(CommandPool&&) = delete;
		CommandPool& operator=(const CommandPool&) = delete;
		CommandPool& operator=(CommandPool&&) = delete;

		void Cleanup();

		const VkCommandPool& GetNativeCommandPool() const;
		VkCommandPool GetNativeCommandPool();

		void AllocateCommandBuffer(VkCommandBuffer* pCommandBuffer) const;
		void FreeCommandBuffer(VkCommandBuffer* pCommandBuffer) const;

		VkCommandBuffer BeginSingleTimeCommands() const;
		void EndSingleTimeCommands(VkCommandBuffer commandBuffer) const;

	private:
		VkDevice mDevice;
		VkQueue mQueue;
		VkCommandPool mCommandPool;

	};

}




