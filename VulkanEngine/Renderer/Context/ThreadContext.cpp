#include "ThreadContext.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Renderer/Common/RendererSettings.hpp"

#include <stdexcept>

namespace Renderer
{
	ThreadContext::ThreadContext(const std::unique_ptr<Device>& device)
	{
		mCommandBuffers.clear();
		mSyncObjects.clear();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			CommandBuffer commandBuffer(device);
			mCommandBuffers.push_back(std::move(commandBuffer));
			SyncObject syncObject(device);
			mSyncObjects.push_back(std::move(syncObject));
		}
		LOGINFO("Thread context Initialized");
	}

	void ThreadContext::Cleanup(const std::unique_ptr<Device>& device)
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			mCommandBuffers[i].Cleanup(device);
			mSyncObjects[i].Cleanup();
		}
		mCommandBuffers.clear();
		mSyncObjects.clear();
		LOGINFO("Thread context cleaned");
	}

	void ThreadContext::NextFrame()
	{
		mFrameIndex = (mFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	uint32_t ThreadContext::GetFrame() const
	{
		return mFrameIndex;
	}

	CommandBuffer& ThreadContext::GetCommandBuffer()
	{
		return mCommandBuffers[mFrameIndex];
	}

	SyncObject& ThreadContext::GetSyncObject()
	{
		return mSyncObjects[mFrameIndex];
	}

}


