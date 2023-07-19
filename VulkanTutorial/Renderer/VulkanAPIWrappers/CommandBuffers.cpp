#include "CommandBuffers.h"

namespace Renderer
{
	CommandBuffers::CommandBuffers()
	{
	}

	CommandBuffers::~CommandBuffers()
	{
	}

	void CommandBuffers::Initialize(Device& device, CommandPool& commandPool)
	{
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = commandPool.GetCommandPool();
		allocateInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		if (vkAllocateCommandBuffers(device.GetDevice(), &allocateInfo, mCommandBuffers) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffer!");
		}
		std::cout << "Command buffer created" << std::endl;
	}

	void CommandBuffers::Cleanup(Device& device)
	{
	}

	VkCommandBuffer& CommandBuffers::GetCommandBuffer(size_t index)
	{
		return mCommandBuffers[index];
	}

	void CommandBuffers::RecordCommandBuffer(VkCommandBuffer commandBuffer, VkFramebuffer frameBuffer, SwapChain& swapChain, RenderPass& renderPass, Pipeline& pipeline)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to begin command buffer");
		}
		auto swapChainExtend = swapChain.GetSwapChainExtend();
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass.GetRenderPass();
		renderPassInfo.framebuffer = frameBuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = swapChainExtend;
		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChainExtend.width;
		viewport.height = (float)swapChainExtend.height;
		viewport.minDepth = 0.0;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChainExtend;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to end command buffer!");
		}
	}



}


