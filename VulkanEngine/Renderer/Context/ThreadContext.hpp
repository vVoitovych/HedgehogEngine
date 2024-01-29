#pragma once

#include <vector>
#include <memory>

namespace Renderer
{
	class VulkanContext;
	class CommandBuffer;
	class SyncObject;

	class ThreadContext
	{
	public:
		ThreadContext(const std::unique_ptr<VulkanContext>& vulkanContext);
		~ThreadContext();

		ThreadContext(const ThreadContext&) = delete;
		ThreadContext& operator=(const ThreadContext&) = delete;

		void Cleanup(const std::unique_ptr<VulkanContext>& vulkanContext);

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



