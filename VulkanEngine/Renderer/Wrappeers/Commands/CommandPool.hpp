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

		void Cleanup(const std::unique_ptr<Device>& device);

		const VkCommandPool& GetNativeCommandPool() const;

		void AllocateCommandBuffer(const std::unique_ptr<Device>& device, VkCommandBuffer* pCommandBuffer) const;
		void FreeCommandBuffer(const std::unique_ptr<Device>& device, VkCommandBuffer* pCommandBuffer) const;

	private:
		VkCommandPool mCommandPool;

	};

}




