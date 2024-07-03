#include "CommandBuffer.hpp"
#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/SwapChain/SwapChain.hpp"
#include "Wrappeers/RenderPass/RenderPass.hpp"
#include "Wrappeers/Pipeline/Pipeline.hpp"
#include "Common/EngineDebugBreak.hpp"

#include "Logger/Logger.hpp"

#include <array>

namespace Renderer
{
	CommandBuffer::CommandBuffer(const Device& device)
		:mCommandBuffer(nullptr)
	{
		device.AllocateCommandBuffer(&mCommandBuffer);
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

	void CommandBuffer::Cleanup(const Device& device)
	{
		device.FreeCommandBuffer(&mCommandBuffer);
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

	void CommandBuffer::BeginSingleTimeCommands() const
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(mCommandBuffer, &beginInfo);
	}

	void CommandBuffer::EndSingleTimeCommands(const Device& device) const
	{
		vkEndCommandBuffer(mCommandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &mCommandBuffer;

		vkQueueSubmit(device.GetNativeGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(device.GetNativeGraphicsQueue());

	}

	void CommandBuffer::BeginRenderPass(VkExtent2D extend, const RenderPass& renderPass, VkFramebuffer frameBuffer)
	{
		std::array<VkClearValue, 2> clearValues{};
		clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 };


		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass.GetNativeRenderPass();
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

	void CommandBuffer::BindPipeline(const Pipeline& pipeline, VkPipelineBindPoint bindPoint)
	{
		vkCmdBindPipeline(mCommandBuffer, bindPoint, pipeline.GetNativePipeline());
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
		const Pipeline& pipeline, 
		uint32_t firstSet, uint32_t setsCount,
		VkDescriptorSet* descriptorSets, 
		uint32_t dynamicOffsetCount, 
		uint32_t* pDynamicOffsets)
	{
		vkCmdBindDescriptorSets(mCommandBuffer, bindPoint, pipeline.GetNativePipelineLayout(), firstSet, setsCount, descriptorSets, dynamicOffsetCount, pDynamicOffsets);
	}

	void CommandBuffer::PushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues)
	{
		vkCmdPushConstants(mCommandBuffer, layout, stageFlags, offset, size, pValues);
	}

	void CommandBuffer::Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
	{
		vkCmdDraw(mCommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
	}

	void CommandBuffer::DrawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
	{
		vkCmdDrawIndexed(mCommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}

	void CommandBuffer::CopyImageToImage(VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize)
	{
		VkImageBlit2 blitRegion{ };
		blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
		blitRegion.pNext = nullptr;

		blitRegion.srcOffsets[1].x = srcSize.width;
		blitRegion.srcOffsets[1].y = srcSize.height;
		blitRegion.srcOffsets[1].z = 1;

		blitRegion.dstOffsets[1].x = dstSize.width;
		blitRegion.dstOffsets[1].y = dstSize.height;
		blitRegion.dstOffsets[1].z = 1;

		blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blitRegion.srcSubresource.baseArrayLayer = 0;
		blitRegion.srcSubresource.layerCount = 1;
		blitRegion.srcSubresource.mipLevel = 0;

		blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blitRegion.dstSubresource.baseArrayLayer = 0;
		blitRegion.dstSubresource.layerCount = 1;
		blitRegion.dstSubresource.mipLevel = 0;

		VkBlitImageInfo2 blitInfo{ };
		blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
		blitInfo.pNext = nullptr;
		blitInfo.dstImage = destination;
		blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		blitInfo.srcImage = source;
		blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		blitInfo.filter = VK_FILTER_LINEAR;
		blitInfo.regionCount = 1;
		blitInfo.pRegions = &blitRegion;

		vkCmdBlitImage2(mCommandBuffer, &blitInfo);
	}

	void CommandBuffer::CopyBufferToBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkBufferCopy copyRegion{};
		copyRegion.size = size;
		vkCmdCopyBuffer(mCommandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
	}

	void CommandBuffer::CopyBufferToImage(VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(mCommandBuffer, srcBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	void CommandBuffer::ClearColorImage(VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
	{
		vkCmdClearColorImage(mCommandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
	}

	void CommandBuffer::ClearDepthStencilImage(VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, 
													uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
	{
		vkCmdClearDepthStencilImage(mCommandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
	}

	VkImageSubresourceRange ImageSubresourceRange(VkImageAspectFlags aspectMask)
	{
		VkImageSubresourceRange subImage{};
		subImage.aspectMask = aspectMask;
		subImage.baseMipLevel = 0;
		subImage.levelCount = VK_REMAINING_MIP_LEVELS;
		subImage.baseArrayLayer = 0;
		subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

		return subImage;

	}
	void CommandBuffer::TransitionImage(VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
	{
		VkImageMemoryBarrier2 imageBarrier = {};
		imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		imageBarrier.pNext = nullptr;

		imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

		imageBarrier.oldLayout = currentLayout;
		imageBarrier.newLayout = newLayout;

		VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		imageBarrier.subresourceRange = ImageSubresourceRange(aspectMask);
		imageBarrier.image = image;

		VkDependencyInfo depInfo{};
		depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		depInfo.pNext = nullptr;

		depInfo.imageMemoryBarrierCount = 1;
		depInfo.pImageMemoryBarriers = &imageBarrier;

		vkCmdPipelineBarrier2(mCommandBuffer, &depInfo);
	}

}


