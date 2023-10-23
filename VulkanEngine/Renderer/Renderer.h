#pragma once

#include "VulkanEngine/Renderer/Common/RendererSettings.h"
#include "Device/Device.h"
#include "SwapChain/SwapChain.h"
#include "SyncObjects/SyncObjects.h"
#include "RenderPass/RenderPass.h"
#include "Pipeline/Pipeline.h"
#include "FrameBuffer/FrameBuffers.h"
#include "Commands/CommandBuffer.h"
#include "Descriptors/UBO.h"
#include "Descriptors/DescriptorSetLayout.h"
#include "Descriptors/DescriptorSet.h"
#include "WindowManagment/WindowManager.h"
#include "Resources/Mesh/Mesh.h"
#include "Resources/TextureImage/TextureImage.h"
#include "Resources/TextureImage/TextureSampler.h"
#include "Resources/DepthBuffer/DepthBuffer.h"

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

		void UpdateUniformBuffer();
		void DrawFrame();
		void RecreateSwapChain();

		bool ShouldClose();


	private:

		uint32_t currentFrame = 0;
		bool mShouldClose = false;

		WindowManager mWindowManager;
		Device mDevice;
		SwapChain mSwapChain;
		SyncObjects mSyncObjects;
		DepthBuffer mDepthBuffer;
		RenderPass mRenderPass;
		Pipeline mPipeline;
		FrameBuffers mFrameBuffers;
		CommandBuffer mCommandBuffers[MAX_FRAMES_IN_FLIGHT];
		UBO mUniformBuffers[MAX_FRAMES_IN_FLIGHT];
		DescriptorSetLayout mDescriptorSetLayout;
		DescriptorSet mDescriptorSets[MAX_FRAMES_IN_FLIGHT];
		Mesh mMesh;
		TextureImage mTextureImage;
		TextureSampler mTextureSampler;

	};
}




