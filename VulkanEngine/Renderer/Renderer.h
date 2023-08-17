#pragma once

#include "Common/pch.h"
#include "Device/Device.h"
#include "SwapChain/SwapChain.h"
#include "SyncObjects/SyncObjects.h"
#include "RenderPass/RenderPass.h"
#include "Pipeline/Pipeline.h"
#include "FrameBuffer/FrameBuffers.h"
#include "Commands/CommandPool.h"
#include "Commands/CommandBuffer.h"

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
		Device mDevice;
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




