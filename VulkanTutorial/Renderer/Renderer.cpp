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
		mCommandBuffers.Initialize(mDevice, mCommandPool);
	}

	void Renderer::Cleanup()
	{
		mCommandBuffers.Cleanup(mDevice);
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

	void Renderer::DrawFrame()
	{
		mSyncObjects.WaitforInFlightFence(mDevice, currentFrame);

		uint32_t imageIndex;
		vkAcquireNextImageKHR(mDevice.GetDevice(), mSwapChain.GetSwapChain(), UINT64_MAX, mSyncObjects.GetImageAvailableSemaphore(currentFrame), VK_NULL_HANDLE, &imageIndex);
		vkResetCommandBuffer(mCommandBuffers.GetCommandBuffers()[currentFrame], 0);
		CommandBuffers::RecordCommandBuffer(mCommandBuffers.GetCommandBuffers()[currentFrame], mFrameBuffers.GetFrameBuffer(imageIndex), mSwapChain, mRenderPass, mPipeline);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { mSyncObjects.GetImageAvailableSemaphore(currentFrame) };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &mCommandBuffers.GetCommandBuffers()[currentFrame];
		VkSemaphore signalSemaphores[] = { mSyncObjects.GetRenderFinishedSemaphore(currentFrame) };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(mDevice.GetGraphicsQueue(), 1, &submitInfo, mSyncObjects.GetInFlightFence(currentFrame)) != VK_SUCCESS)
		{
			throw std::runtime_error("fsailed to submit draw command buffer!");
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

		vkQueuePresentKHR(mDevice.GetPresentQueue(), &presentInfo);

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}
}



