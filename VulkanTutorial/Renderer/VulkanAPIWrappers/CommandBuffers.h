#pragma once

#include "../Common/pch.h"
#include "Device.h"
#include "CommandPool.h"
#include "SwapChain.h"
#include "RenderPass.h"
#include "Pipeline.h"

namespace Renderer
{
	class CommandBuffers
	{
	public:
		CommandBuffers();
		~CommandBuffers();

		CommandBuffers(const CommandBuffers&) = delete;
		CommandBuffers& operator=(const CommandBuffers&) = delete;

		void Initialize(Device& device, CommandPool& commandPool);
		void Cleanup(Device& device);

		std::vector<VkCommandBuffer> GetCommandBuffers();

		static void RecordCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer frameBuffer, SwapChain& swapChain, RenderPass& renderPass, Pipeline& pipeline);
	private:
		std::vector<VkCommandBuffer> mCommandBuffers;
	};
}


