#pragma once

#include "../Common/pch.h"

#include "Device.h"

namespace Renderer
{
	class CommandPool
	{
	public:
		CommandPool();
		~CommandPool();

		CommandPool(const CommandPool&) = delete;
		CommandPool& operator=(const CommandPool&) = delete;

		void Initialize(Device& device);
		void Cleanup(Device& device);

		VkCommandPool GetCommandPool();
	private:
		VkCommandPool mCommandPool;
	};
}

