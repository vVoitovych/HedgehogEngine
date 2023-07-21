#pragma once

#include "Common/pch.h"
#include "VulkanAPIWrappers/Instance.h"
#include "VulkanAPIWrappers/Device.h"
#include "VulkanAPIWrappers/DebugMessenger.h"
#include "VulkanAPIWrappers/Surface.h"
#include "VulkanAPIWrappers/SwapChain.h"
#include "VulkanAPIWrappers/SyncObjects.h"
#include "VulkanAPIWrappers/RenderPass.h"
#include "VulkanAPIWrappers/Pipeline.h"
#include "VulkanAPIWrappers/FrameBuffers.h"
#include "VulkanAPIWrappers/CommandPool.h"
#include "VulkanAPIWrappers/CommandBuffer.h"

#include "WindowManagment/WindowManager.h"
#include "Mesh/Mesh.h"

namespace Renderer
{
	class Renderer
	{
	public:
		Renderer();
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		void Initialize(WindowManager& windowManager);
		void Cleanup();

		void DrawFrame(WindowManager& windowManager);
		void RecreateSwapChain(WindowManager& windowManager);

	private:
		void CleanupSwapChain();
		void CreateSwapShain(WindowManager& windowManager);
		void CreateFrameBuffers();

	private:

		uint32_t currentFrame = 0;

		Instance mInstance;
		Surface mSurface;
		Device mDevice;
		DebugMessenger mDebugMessenger;
		SwapChain mSwapChain;
		SyncObjects mSyncObjects;
		RenderPass mRenderPass;
		Pipeline mPipeline;
		FrameBuffers mFrameBuffers;
		CommandPool mCommandPool;
		CommandBuffer mCommandBuffers[MAX_FRAMES_IN_FLIGHT];

		Mesh mMesh;
	};
}




