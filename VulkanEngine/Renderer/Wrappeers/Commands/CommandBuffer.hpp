#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;
	class CommandPool;
	class RenderPass;
	class Pipeline;

	class CommandBuffer
	{
	public:
		CommandBuffer(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& commandPool);
		~CommandBuffer();

		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer& operator=(const CommandBuffer&) = delete;

		CommandBuffer(CommandBuffer&& other);
		CommandBuffer& operator=(CommandBuffer&& other);

		void Cleanup(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& commandPool);

		VkCommandBuffer& GetNativeCommandBuffer();

		void BeginCommandBuffer(VkCommandBufferUsageFlags flags);
		void EndCommandBuffer();

		void BeginSingleTimeCommands() const;
		void EndSingleTimeCommands(const std::unique_ptr<Device>& device) const;

		void BeginRenderPass(VkExtent2D extend, std::unique_ptr<RenderPass>& renderPass, VkFramebuffer frameBuffer);
		void EndRenderPass();

		void BindPipeline(std::unique_ptr<Pipeline>& pipeline, VkPipelineBindPoint bindPoint);
		void SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
		void SetScissor(VkOffset2D offset, VkExtent2D extend);
		void BindVertexBuffers(uint32_t firstBinding, uint32_t bindingsCount, VkBuffer* buffers, VkDeviceSize* offsets);
		void BindIndexBuffer(VkBuffer indexBuffer, VkDeviceSize offset, VkIndexType indexType);
		void BindDescriptorSers(VkPipelineBindPoint bindPoint, std::unique_ptr<Pipeline>& pipeline, uint32_t firstSet, uint32_t setsCount,
			VkDescriptorSet* descriptorSets, uint32_t dynamicOffsetCount, uint32_t* pDynamicOffsets);
		void PushConstants(VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues);
		void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void DrawIndexed(uint32_t IndexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

		void CopyImageToImage(VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
		void ClearColorImage(VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
		void ClearDepthStencilImage(VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges);
		void RecordTransitionImageLayout(
            const uint32_t mipLevels,
            const VkImageLayout& oldLayout,
            const VkImageLayout& newLayout,
            const bool isCubemap,
            const VkImage& image
        );
	
	private:
		VkCommandBuffer mCommandBuffer;
	};
}


