#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class CommandPool;
	class RenderPass;
	class Pipeline;

	class CommandBuffer
	{
	public:
		CommandBuffer(const std::unique_ptr<CommandPool>& cxommandPool);
		~CommandBuffer();

		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer& operator=(const CommandBuffer&) = delete;


		CommandBuffer(CommandBuffer&& other);
		CommandBuffer& operator=(CommandBuffer&& other);

		void Cleanup(const std::unique_ptr<CommandPool>& commandPool);

		VkCommandBuffer& GetNativeCommandBuffer();

		void BeginCommandBuffer(VkCommandBufferUsageFlags flags);
		void EndCommandBuffer();
		void BeginRenderPass(VkExtent2D extend, std::unique_ptr<RenderPass>& renderPass, VkFramebuffer frameBuffer);
		void EndRenderPass();
		void BindPipeline(std::unique_ptr<Pipeline>& pipeline, VkPipelineBindPoint bindPoint);
		void SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
		void SetScissor(VkOffset2D offset, VkExtent2D extend);
		void BindVertexBuffers(uint32_t firstBinding, uint32_t bindingsCount, VkBuffer* buffers, VkDeviceSize* offsets);
		void BindIndexBuffer(VkBuffer indexBuffer, VkDeviceSize offset, VkIndexType indexType);
		void BindDescriptorSers(VkPipelineBindPoint bindPoint, std::unique_ptr<Pipeline>& pipeline, uint32_t firstSet, uint32_t setsCount,
			VkDescriptorSet* descriptorSets, uint32_t dynamicOffsetCount, uint32_t* pDynamicOffsets);
		void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void DrawIndexed(uint32_t IndexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

	private:
		VkCommandBuffer mCommandBuffer;
	};
}


