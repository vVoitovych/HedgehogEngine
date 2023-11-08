#pragma once

#include "VulkanEngine/Renderer/Common/RendererSettings.hpp"
#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/SwapChain/SwapChain.hpp"
#include "Wrappeers/SyncObjects/SyncObjects.hpp"
#include "Wrappeers/RenderPass/RenderPass.hpp"
#include "Wrappeers/Pipeline/Pipeline.hpp"
#include "Wrappeers/FrameBuffer/FrameBuffers.hpp"
#include "Wrappeers/Commands/CommandBuffer.hpp"
#include "Wrappeers/Descriptors/UBO.hpp"
#include "Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "Wrappeers/Descriptors/DescriptorSet.hpp"
#include "WindowManagment/WindowManager.hpp"
#include "Resources/Mesh/Mesh.hpp"
#include "Resources/TextureImage/TextureImage.hpp"
#include "Resources/TextureImage/TextureSampler.hpp"
#include "Resources/DepthBuffer/DepthBuffer.hpp"
#include "Camera/Camera.hpp"

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

		void HandleInput();
		void Update(float dt);

		void UpdateUniformBuffer();
		void DrawFrame();
		void RecreateSwapChain();

		bool ShouldClose();

		float GetFrameTime();

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

		Camera mCamera;
	};
}




