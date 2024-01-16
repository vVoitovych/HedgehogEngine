#include "PresentPass.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/ThreadContext.hpp"
#include "Renderer/Context/EngineContext.hpp"
#include "Renderer/WindowManagment/WindowManager.hpp"
#include "Renderer/Wrappeers/Commands/CommandBuffer.hpp"

#include <stdexcept>

namespace Renderer
{
	PresentPass::PresentPass(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain)
	{
		mDevice = device->GetNativeDevice();
		mSwapChain = swapChain->GetNativeSwapChain();
		mGraphicQueue = device->GetNativeGraphicsQueue();
		mPresentQueue = device->GetNativePresentQueue();
	}

	void PresentPass::Render(std::unique_ptr<RenderContext>& renderContext)
	{
		if (!mDevice.has_value() || !mSwapChain.has_value() || !mGraphicQueue.has_value() || !mPresentQueue.has_value())
		{
			throw std::runtime_error("Present pass doesn't initialized!");
		}
		auto [engineContext, frameContext, threadContext] = renderContext->GetContexts();
		auto& syncObject = threadContext->GetSyncObject();
		auto& commandBuffer = threadContext->GetCommandBuffer();

		commandBuffer.EndCommandBuffer();

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { syncObject.GetImageAvailableSemaphore() };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer.GetNativeCommandBuffer();
		VkSemaphore signalSemaphores[] = { syncObject.GetRenderFinishedSemaphore() };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(mGraphicQueue.value(), 1, &submitInfo, syncObject.GetInFlightFence()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}
		auto index = engineContext->GetBackBufferIndex();
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapChains[] = { mSwapChain.value() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &index;
		presentInfo.pResults = nullptr;

		VkResult result = vkQueuePresentKHR(mPresentQueue.value(), &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || engineContext->GetWindowManager()->IsWindowResized())
		{
			engineContext->GetWindowManager()->ResetResizedState();
			engineContext->ResizeWindow();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image!");
		}
		threadContext->NextFrame();
	}

	void PresentPass::Cleanup(const std::unique_ptr<Device>& device)
	{
	}

	void PresentPass::SetSwapChain(const std::unique_ptr<SwapChain>& swapChain)
	{
		mSwapChain = swapChain->GetNativeSwapChain();
	}


}



