#include "CommandBuffer.hpp"
#include "CommandPool.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "Renderer/Wrappeers/Pipeline/Pipeline.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"

#include <array>

namespace Renderer
{
	CommandBuffer::CommandBuffer(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& commandPool)
		:mCommandBuffer(nullptr)
	{
		commandPool->AllocateCommandBuffer(device, &mCommandBuffer);
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

	void CommandBuffer::Cleanup(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& cxommandPool)
	{
		cxommandPool->FreeCommandBuffer(device, &mCommandBuffer);
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

	void CommandBuffer::EndSingleTimeCommands(const std::unique_ptr<Device>& device) const
	{
		vkEndCommandBuffer(mCommandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &mCommandBuffer;

		vkQueueSubmit(device->GetNativeGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(device->GetNativeGraphicsQueue());

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

	void CommandBuffer::ClearColorImage(VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
	{
		vkCmdClearColorImage(mCommandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
	}

	void CommandBuffer::ClearDepthStencilImage(VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, 
													uint32_t rangeCount, const VkImageSubresourceRange* pRanges)
	{
		vkCmdClearDepthStencilImage(mCommandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
	}

	void CreateImageMemoryBarrier(
		const uint32_t mipLevels,
		const VkImageLayout& oldLayout,
		const VkImageLayout& newLayout,
		const bool isCubemap,
		const VkImage& image,
		VkImageMemoryBarrier& imgMemoryBarrier,
		VkPipelineStageFlags& sourceStage,
		VkPipelineStageFlags& destinationStage)
	{
		imgMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imgMemoryBarrier.oldLayout = oldLayout;
		imgMemoryBarrier.newLayout = newLayout;
		imgMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imgMemoryBarrier.image = image;
		imgMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imgMemoryBarrier.subresourceRange.baseMipLevel = 0;
		imgMemoryBarrier.subresourceRange.baseArrayLayer = 0;

		if (isCubemap)
			imgMemoryBarrier.subresourceRange.layerCount = 6;
		else
			imgMemoryBarrier.subresourceRange.layerCount = 1;

		imgMemoryBarrier.subresourceRange.levelCount = mipLevels;

		imgMemoryBarrier.srcAccessMask = 0;
		imgMemoryBarrier.dstAccessMask = 0;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
			newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			) {

			imgMemoryBarrier.srcAccessMask = 0;
			imgMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
			newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			) {

			imgMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			imgMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
			newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			) {

			imgMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imgMemoryBarrier.dstAccessMask = (
				VK_ACCESS_HOST_WRITE_BIT |
				VK_ACCESS_TRANSFER_WRITE_BIT
				);

			sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
			newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			) {

			imgMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imgMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
			newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
			) {

			imgMemoryBarrier.srcAccessMask = 0;
			imgMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		}
		else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
			newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
			) {

			imgMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			imgMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

		}
		else
			throw std::invalid_argument("Unsupported layout transition!");
	}

	void CommandBuffer::RecordTransitionImageLayout(const uint32_t mipLevels, const VkImageLayout& oldLayout, const VkImageLayout& newLayout, 
													const bool isCubemap, const VkImage& image)
	{
		VkImageMemoryBarrier imgMemoryBarrier{};
		VkPipelineStageFlags sourceStage, destinationStage;
		CreateImageMemoryBarrier(
			mipLevels,
			oldLayout,
			newLayout,
			isCubemap,
			image,
			imgMemoryBarrier,
			sourceStage,
			destinationStage
		);

		vkCmdPipelineBarrier(
			mCommandBuffer,
			sourceStage, destinationStage,
			0,
			0, nullptr,
			0, nullptr,
			1, &imgMemoryBarrier
		);
	}


}


