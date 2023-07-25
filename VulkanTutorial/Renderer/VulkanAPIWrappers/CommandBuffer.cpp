#include "CommandBuffer.h"

namespace Renderer
{
	CommandBuffer::CommandBuffer()
	{
	}

	CommandBuffer::~CommandBuffer()
	{
	}

	void CommandBuffer::Initialize(Device& device, CommandPool& commandPool)
	{
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = commandPool.GetCommandPool();
		allocateInfo.commandBufferCount = 1;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		if (vkAllocateCommandBuffers(device.GetDevice(), &allocateInfo, &mCommandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffer!");
		}
		std::cout << "Command buffer created" << std::endl;
	}

	void CommandBuffer::Cleanup(Device& device)
	{
	}

	VkCommandBuffer& CommandBuffer::GetCommandBuffer()
	{
		return mCommandBuffer;
	}

	void CommandBuffer::BeginCommandBuffer(VkCommandBufferUsageFlags flags)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = flags;
		beginInfo.pInheritanceInfo = nullptr;

		if (vkBeginCommandBuffer(mCommandBuffer, &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to begin command buffer");
		}
	}

	void CommandBuffer::EndCommandBuffer()
	{
		if (vkEndCommandBuffer(mCommandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to end command buffer!");
		}
	}

	void CommandBuffer::BeginRenderPass(VkExtent2D extend, RenderPass& renderPass, VkFramebuffer frameBuffer)
	{
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass.GetRenderPass();
		renderPassInfo.framebuffer = frameBuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = extend;
		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(mCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void CommandBuffer::EndRenderPass()
	{
		vkCmdEndRenderPass(mCommandBuffer);
	}

	void CommandBuffer::BindPipeline(Pipeline& pipeline, VkPipelineBindPoint bindPoint)
	{
		vkCmdBindPipeline(mCommandBuffer, bindPoint, pipeline.GetPipeline());
	}

	void CommandBuffer::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
	{
		VkViewport viewport{};
		viewport.x = x;
		viewport.y = y;
		viewport.width = width;
		viewport.height = height;
		viewport.minDepth = minDepth;
		viewport.maxDepth = maxDepth;
		vkCmdSetViewport(mCommandBuffer, 0, 1, &viewport);
	}

	void CommandBuffer::SetScissor(VkOffset2D offset, VkExtent2D extend)
	{
		VkRect2D scissor{};
		scissor.offset = offset;
		scissor.extent = extend;
		vkCmdSetScissor(mCommandBuffer, 0, 1, &scissor);
	}

	void CommandBuffer::BindVertexBuffers(uint32_t firstBinding, uint32_t bindingsCount, VkBuffer* buffers, VkDeviceSize* offsets)
	{
		vkCmdBindVertexBuffers(mCommandBuffer, firstBinding, bindingsCount, buffers, offsets);
	}

	void CommandBuffer::BindIndexBuffer(VkBuffer indexBuffer, VkDeviceSize offset, VkIndexType indexType)
	{
		vkCmdBindIndexBuffer(mCommandBuffer, indexBuffer, offset, indexType);
	}

	void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		vkCmdDraw(mCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		vkCmdDrawIndexed(mCommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}


}


