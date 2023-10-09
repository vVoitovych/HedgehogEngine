#include "Renderer.h"
#include "VulkanEngine/Logger/Logger.h"

namespace Renderer
{
	Renderer::Renderer()
	{
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::Initialize()
	{
		mWindowManager.Initialize(WindowState::GetDefaultState());
		mDevice.Initialize(mWindowManager);
		mSwapChain.Initialize(mDevice, mWindowManager);
		mSyncObjects.Initialize(mDevice);
		mRenderPass.Initialize(mDevice, mSwapChain.GetFormat());
		mCommandPool.Initialize(mDevice);
		mDescriptorPool.Initialize(mDevice);
		mDescriptorSetLayout.Initialize(mDevice);
		mPipeline.Initialize(mDevice, mSwapChain, mRenderPass, mDescriptorSetLayout);
		mFrameBuffers.Initialize(mDevice, mSwapChain, mRenderPass);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			mCommandBuffers[i].Initialize(mDevice, mCommandPool);
			mUniformBuffers[i].Initialize(mDevice);
			mDescriptorSets[i].Initialize(mDevice, mDescriptorPool, mDescriptorSetLayout, mUniformBuffers[i]);
		}
		mMesh.Initialize(mDevice, mCommandPool);
	}
	 
	void Renderer::Cleanup()
	{
		vkQueueWaitIdle(mDevice.GetNativeGraphicsQueue());

		mMesh.Cleanup();
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			mDescriptorSets[i].Cleanup();
			mUniformBuffers[i].Cleanup();
			mCommandBuffers[i].Cleanup();
		}
		mDescriptorSetLayout.Cleanup();
		mDescriptorPool.Cleanup();
		mFrameBuffers.Cleanup();
		mCommandPool.Cleanup();
		mPipeline.Cleanup();
		mRenderPass.Cleanup();
		mSyncObjects.Cleanup();
		mSwapChain.Cleanup();
		mDevice.Cleanup();
		mWindowManager.Cleanup();
	}

	void Renderer::UpdateUniformBuffer()
	{
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
		auto extend = mSwapChain.GetSwapChainExtend();
		mUniformBuffers[currentFrame].UpdateUniformBuffer(time, extend.width / (float)extend.height);
	}

	void Renderer::DrawFrame()
	{
		mSyncObjects.WaitforInFlightFence(currentFrame);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(mDevice.GetNativeDevice(), mSwapChain.GetNativeSwapChain(), UINT64_MAX, mSyncObjects.GetImageAvailableSemaphore(currentFrame), VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		mSyncObjects.ResetInFlightFence(currentFrame);

		auto& commandBuffer = mCommandBuffers[currentFrame];
		auto frameBuffer = mFrameBuffers.GetNativeFrameBuffer(imageIndex);
		vkResetCommandBuffer(commandBuffer.GetNativeCommandBuffer(), 0);

		auto extend = mSwapChain.GetSwapChainExtend();
		commandBuffer.BeginCommandBuffer(0);
		commandBuffer.BeginRenderPass(extend, mRenderPass, frameBuffer);
		commandBuffer.BindPipeline(mPipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
		commandBuffer.SetViewport(0.0f, 0.0f, (float)extend.width, (float)extend.height, 0.0f, 0.0f);
		commandBuffer.SetScissor({ 0, 0 }, extend);
		VkBuffer vertexBuffers[] = { mMesh.GetVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		commandBuffer.BindVertexBuffers(0, 1, vertexBuffers, offsets);
		commandBuffer.BindIndexBuffer(mMesh.GetIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);
		commandBuffer.BindDescriptorSers(VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline, 0, 1, mDescriptorSets[currentFrame].GetNativeSet(), 0, nullptr);

		commandBuffer.DrawIndexed(mMesh.GetIndiciesCount(), 1, 0, 0, 0);
		commandBuffer.EndRenderPass();
		commandBuffer.EndCommandBuffer();

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { mSyncObjects.GetImageAvailableSemaphore(currentFrame) };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer.GetNativeCommandBuffer();
		VkSemaphore signalSemaphores[] = { mSyncObjects.GetRenderFinishedSemaphore(currentFrame) };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(mDevice.GetNativeGraphicsQueue(), 1, &submitInfo, mSyncObjects.GetInFlightFence(currentFrame)) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapChains[] = { mSwapChain.GetNativeSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		result = vkQueuePresentKHR(mDevice.GetNativePresentQueue(), &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mWindowManager.IsWindowResized())
		{
			mWindowManager.ResetResizedState();
			RecreateSwapChain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image!");
		}
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void Renderer::RecreateSwapChain()
	{
		vkDeviceWaitIdle(mDevice.GetNativeDevice());

		CleanupSwapChain();

		CreateSwapShain();
		CreateFrameBuffers();
	}


	bool Renderer::ShouldClose()
	{
		return mWindowManager.ShouldClose();
	}

	void Renderer::CleanupSwapChain()
	{
		mFrameBuffers.Cleanup();
		mSwapChain.Cleanup();
	}

	void Renderer::CreateSwapShain()
	{
		mSwapChain.Initialize(mDevice, mWindowManager);
	}

	void Renderer::CreateFrameBuffers()
	{
		mFrameBuffers.Initialize(mDevice, mSwapChain, mRenderPass);
	}

}



