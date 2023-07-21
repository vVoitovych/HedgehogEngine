#include "Renderer.h"

namespace Renderer
{
	Renderer::Renderer()
	{
	}

	Renderer::~Renderer()
	{
	}

	void Renderer::Initialize(WindowManager& windowManager)
	{
		mInstance.Initialize();
		mSurface.Initialize(mInstance, windowManager);
		mDebugMessenger.Initialize(mInstance);
		mDevice.Initialize(mInstance, mSurface);
		mSwapChain.Initialize(mDevice, mSurface, windowManager);
		mSyncObjects.Initialize(mDevice);
		mRenderPass.Initialize(mDevice, mSwapChain.GetFormat());
		mPipeline.Initialize(mDevice, mSwapChain, mRenderPass);
		mFrameBuffers.Initialize(mDevice, mSwapChain, mRenderPass);
		mCommandPool.Initialize(mDevice);
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			mCommandBuffers[i].Initialize(mDevice, mCommandPool);
		}

		mMesh.CreateVertexBuffer(mDevice);
	}

	void Renderer::Cleanup()
	{
		mMesh.Cleanup(mDevice);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			mCommandBuffers[i].Cleanup(mDevice);
		}
		mCommandPool.Cleanup(mDevice);
		mFrameBuffers.Cleanup(mDevice);
		mPipeline.Cleanup(mDevice);
		mRenderPass.Cleanup(mDevice);
		mSyncObjects.Cleanup(mDevice);
		mSwapChain.Cleanup(mDevice);
		mDevice.Cleanup();
		mDebugMessenger.Cleanup(mInstance);
		mSurface.Cleanup(mInstance);
		mInstance.Cleanup();
	}

	void Renderer::DrawFrame(WindowManager& windowManager)
	{
		mSyncObjects.WaitforInFlightFence(mDevice, currentFrame);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(mDevice.GetDevice(), mSwapChain.GetSwapChain(), UINT64_MAX, mSyncObjects.GetImageAvailableSemaphore(currentFrame), VK_NULL_HANDLE, &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			RecreateSwapChain(windowManager);
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}
		mSyncObjects.ResetInFlightFence(mDevice, currentFrame);

		auto& commandBuffer = mCommandBuffers[currentFrame];
		auto frameBuffer = mFrameBuffers.GetFrameBuffer(imageIndex);
		vkResetCommandBuffer(commandBuffer.GetCommandBuffer(), 0);

		auto extend = mSwapChain.GetSwapChainExtend();
		commandBuffer.BeginCommandBuffer(0);
		commandBuffer.BeginRenderPass(extend, mRenderPass, frameBuffer);
		commandBuffer.BindPipeline(mPipeline, VK_PIPELINE_BIND_POINT_GRAPHICS);
		commandBuffer.SetViewport(0.0f, 0.0f, extend.width, extend.height, 0.0f, 0.0f);
		commandBuffer.SetScissor({ 0, 0 }, extend);
		VkBuffer vertexBuffers[] = { mMesh.GetVertexBuffer() };
		VkDeviceSize offsets[] = { 0 };
		commandBuffer.BindVertexBuffers(0, 1, vertexBuffers, offsets);
		commandBuffer.Draw(3, 1, 0, 0);
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
		submitInfo.pCommandBuffers = &commandBuffer.GetCommandBuffer();
		VkSemaphore signalSemaphores[] = { mSyncObjects.GetRenderFinishedSemaphore(currentFrame) };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(mDevice.GetGraphicsQueue(), 1, &submitInfo, mSyncObjects.GetInFlightFence(currentFrame)) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapChains[] = { mSwapChain.GetSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;

		result = vkQueuePresentKHR(mDevice.GetPresentQueue(), &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || windowManager.IsWindowResized())
		{
			windowManager.ResetResizedState();
			RecreateSwapChain(windowManager);
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image!");
		}
		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	void Renderer::RecreateSwapChain(WindowManager& windowManager)
	{
		vkDeviceWaitIdle(mDevice.GetDevice());

		CleanupSwapChain();

		CreateSwapShain(windowManager);
		CreateFrameBuffers();
	}


	void Renderer::CleanupSwapChain()
	{
		mFrameBuffers.Cleanup(mDevice);
		mSwapChain.Cleanup(mDevice);
	}

	void Renderer::CreateSwapShain(WindowManager& windowManager)
	{
		mSwapChain.Initialize(mDevice, mSurface, windowManager);
	}

	void Renderer::CreateFrameBuffers()
	{
		mFrameBuffers.Initialize(mDevice, mSwapChain, mRenderPass);
	}

}



