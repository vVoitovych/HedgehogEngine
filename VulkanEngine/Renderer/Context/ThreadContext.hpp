#pragma once

#include "VulkanEngine/Renderer/Wrappeers/Commands/CommandBuffer.hpp"
#include "VulkanEngine/Renderer/Wrappeers/SyncObjects/SyncObject.hpp"

#include <vector>
#include <memory>

namespace Renderer
{
	class Device;

	class ThreadContext
	{
	public:
		ThreadContext(const std::unique_ptr<Device>& device);
		~ThreadContext() = default;

		ThreadContext(const ThreadContext&) = delete;
		ThreadContext& operator=(const ThreadContext&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);

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



