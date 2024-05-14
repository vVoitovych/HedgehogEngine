#pragma once

#include <vector>

namespace Renderer
{
	class VulkanContext;
	class CommandBuffer;
	class SyncObject;

	class ThreadContext
	{
	public:
		ThreadContext(const VulkanContext& vulkanContext);
		~ThreadContext();

		ThreadContext(const ThreadContext&) = delete;
		ThreadContext& operator=(const ThreadContext&) = delete;

		void Cleanup(const VulkanContext& vulkanContext);

		void NextFrame();
		uint32_t GetFrame() const;

		CommandBuffer& GetCommandBuffer();
		SyncObject& GetSyncObject();

	private:
		std::vector<CommandBuffer> mCommandBuffers;
		std::vector<SyncObject> mSyncObjects;

		uint32_t mFrameIndex = 0;
	};

}



