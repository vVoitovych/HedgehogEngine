#include "ThreadContext.hpp"
#include "VulkanContext.hpp"

#include "Renderer/Wrappeers/Commands/CommandBuffer.hpp"
#include "Renderer/Wrappeers/SyncObjects/SyncObject.hpp"
#include "Renderer/Context/VulkanContext.hpp"

#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Renderer/Common/RendererSettings.hpp"

#include <stdexcept>

namespace Renderer
{
	ThreadContext::ThreadContext(const VulkanContext& vulkanContext)
	{
		mCommandBuffers.clear();
		mSyncObjects.clear();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			CommandBuffer commandBuffer(vulkanContext.GetDevice());
			mCommandBuffers.push_back(std::move(commandBuffer));
			SyncObject syncObject(vulkanContext.GetDevice());
			mSyncObjects.push_back(std::move(syncObject));
		}
		LOGINFO("Thread context Initialized");
	}

	ThreadContext::~ThreadContext()
	{
	}

	void ThreadContext::Cleanup(const VulkanContext& vulkanContext)
	{
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			mCommandBuffers[i].Cleanup(vulkanContext.GetDevice());
			mSyncObjects[i].Cleanup(vulkanContext.GetDevice());
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


