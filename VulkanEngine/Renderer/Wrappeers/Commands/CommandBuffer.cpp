#include "CommandBuffer.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "VulkanEngine/Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Pipeline/Pipeline.hpp"
#include "VulkanEngine/Logger/Logger.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"

#include <array>

namespace Renderer
{
	CommandBuffer::CommandBuffer(const std::unique_ptr<Device>& device)
		:mCommandBuffer(nullptr)
	{
		device->AllocateCommandBuffer(&mCommandBuffer);
		LOGINFO("Command buffer created");
	}

	CommandBuffer::~CommandBuffer()
	{
		if (mCommandBuffer != nullptr)
		{
			LOGERROR("Vulkan command buffer should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	CommandBuffer::CommandBuffer(CommandBuffer&& other)
		: mCommandBuffer(other.mCommandBuffer)
	{
		other.mCommandBuffer = nullptr;
	}

	CommandBuffer& CommandBuffer::operator=(CommandBuffer&& other)
	{
		if (this != &other)
		{
			mCommandBuffer = other.mCommandBuffer;

			other.mCommandBuffer = nullptr;
		}
		return *this;
	}

	void CommandBuffer::Cleanup(const std::unique_ptr<Device>& device)
	{
		device->FreeCommandBuffer(&mCommandBuffer);
		mCommandBuffer = nullptr;
		LOGINFO("Command  buffer cleaned");
	}

	VkCommandBuffer& CommandBuffer::GetNativeCommandBuffer() 
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

	void CommandBuffer::BeginRenderPass(VkExtent2D extend, std::unique_ptr<RenderPass>& renderPass, VkFramebuffer frameBuffer)
	{
		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };


		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass->GetNativeRenderPass();
		renderPassInfo.framebuffer = frameBuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = extend;
		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(mCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void CommandBuffer::EndRenderPass()
	{
		vkCmdEndRenderPass(mCommandBuffer);
	}

	void CommandBuffer::BindPipeline(std::unique_ptr<Pipeline>& pipeline, VkPipelineBindPoint bindPoint)
	{
		vkCmdBindPipeline(mCommandBuffer, bindPoint, pipeline->GetNativePipeline());
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

	void CommandBuffer::BindDescriptorSers(
		VkPipelineBindPoint bindPoint, 
		std::unique_ptr<Pipeline>& pipeline, 
		uint32_t firstSet, uint32_t setsCount,
		VkDescriptorSet* descriptorSets, 
		uint32_t dynamicOffsetCount, 
		uint32_t* pDynamicOffsets)
	{
		vkCmdBindDescriptorSets(mCommandBuffer, bindPoint, pipeline->GetNativePipelineLayout(), firstSet, setsCount, descriptorSets, dynamicOffsetCount, pDynamicOffsets);
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


