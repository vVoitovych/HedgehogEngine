#pragma once

#include "Common/pch.h"
#include "VulkanAPIWrappers/Instance.h"
#include "VulkanAPIWrappers/Device.h"
#include "VulkanAPIWrappers/DebugMessenger.h"
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

		void Initialize();
		void Cleanup();

		void DrawFrame();
		void RecreateSwapChain();

		bool ShouldClose();
	private:
		void CleanupSwapChain();
		void CreateSwapShain();
		void CreateFrameBuffers();

	private:

		uint32_t currentFrame = 0;
		bool mShouldClose = false;

		WindowManager mWindowManager;
		Instance mInstance;
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




