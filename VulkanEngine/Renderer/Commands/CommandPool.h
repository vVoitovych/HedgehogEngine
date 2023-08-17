#pragma once

#include "VulkanEngine/Renderer/Common/pch.h"

namespace Renderer
{
	class Device;

	class CommandPool
	{
	public:
		CommandPool();
		~CommandPool();

		CommandPool(const CommandPool&) = delete;
		CommandPool& operator=(const CommandPool&) = delete;

		void Initialize(Device& device);
		void Cleanup();

		VkCommandPool GetNativeCommandPool();
	private:
		VkDevice mDevice;
		VkCommandPool mCommandPool;
	};
}

