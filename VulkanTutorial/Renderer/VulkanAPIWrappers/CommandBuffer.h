#pragma once

#include "../Common/pch.h"
#include "Device.h"
#include "CommandPool.h"
#include "SwapChain.h"
#include "RenderPass.h"
#include "Pipeline.h"

namespace Renderer
{
	class CommandBuffer
	{
	public:
		CommandBuffer();
		~CommandBuffer();

		CommandBuffer(const CommandBuffer&) = delete;
		CommandBuffer& operator=(const CommandBuffer&) = delete;

		void Initialize(Device& device, CommandPool& commandPool);
		void Cleanup(Device& device);

		VkCommandBuffer& GetCommandBuffer();

		void BeginCommandBuffer(VkCommandBufferUsageFlags flags);
		void EndCommandBuffer();
		void BeginRenderPass(VkExtent2D extend, RenderPass& renderPass, VkFramebuffer frameBuffer);
		void EndRenderPass();
		void BindPipeline(Pipeline& pipeline, VkPipelineBindPoint bindPoint);
		void SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth);
		void SetScissor(VkOffset2D offset, VkExtent2D extend);
		void BindVertexBuffers(uint32_t firstBinding, uint32_t bindingsCount, VkBuffer* buffers, VkDeviceSize* offsets);
		void BindIndexBuffer(VkBuffer indexBuffer, VkDeviceSize offset, VkIndexType indexType);

		void Draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void DrawIndexed(uint32_t IndexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance);

	private:
		VkCommandBuffer mCommandBuffer;
	};
}


