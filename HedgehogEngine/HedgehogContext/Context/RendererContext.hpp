#pragma once

#include <vector>

namespace Wrappers
{
	class CommandBuffer;
	class SyncObject;

}

namespace Context
{
	class VulkanContext;

	class RendererContext
	{
	public:
		RendererContext(const VulkanContext& vulkanContext);
		~RendererContext();

		RendererContext(const RendererContext&) = delete;
		RendererContext& operator=(const RendererContext&) = delete;

		void Cleanup(const VulkanContext& vulkanContext);

		void NextFrame();
		uint32_t GetFrameIndex() const;

		void SetBackBufferIndex(uint32_t index);
		uint32_t GetBackBufferIndex() const;

		Wrappers::CommandBuffer& GetCommandBuffer();
		Wrappers::SyncObject& GetSyncObject();

	private:
		std::vector<Wrappers::CommandBuffer> m_CommandBuffers;
		std::vector<Wrappers::SyncObject> m_SyncObjects;

		uint32_t m_FrameIndex = 0;
		uint32_t m_BackBufferIndex = 0;
	};

}



