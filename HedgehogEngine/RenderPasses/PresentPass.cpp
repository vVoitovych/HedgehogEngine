#include "PresentPass.hpp"

#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/SwapChain/SwapChain.hpp"
#include "Context/RenderContext.hpp"
#include "Context/ThreadContext.hpp"
#include "Context/EngineContext.hpp"
#include "Context/FrameContext.hpp"
#include "Context/VulkanContext.hpp"
#include "ResourceManager/ResourceManager.hpp"

#include "WindowManagment/WindowManager.hpp"
#include "Wrappeers/Commands/CommandBuffer.hpp"
#include "Wrappeers/SyncObjects/SyncObject.hpp"
#include "Wrappeers/Resources/Image/Image.hpp"

#include <stdexcept>

namespace Renderer
{
	PresentPass::PresentPass(const RenderContext& context)
	{
	}

	void PresentPass::Render(RenderContext& context, const ResourceManager& resourceManager)
	{

		auto& engineContext = context.GetEngineContext();
		auto& frameContext = context.GetFrameContext();
		auto& threadContext = context.GetThreadContext();
		auto& vulkanContext = context.GetVulkanContext();

		auto& syncObject = threadContext.GetSyncObject();
		auto& commandBuffer = threadContext.GetCommandBuffer();

		auto backBufferIndex = frameContext.GetBackBufferIndex();
		auto& colorBuffer = resourceManager.GetColorBuffer();

		commandBuffer.TransitionImage(
			colorBuffer.GetNativeImage(),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		commandBuffer.TransitionImage(
			vulkanContext.GetSwapChain().GetSwapChainImage(backBufferIndex),
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		commandBuffer.CopyImageToImage(
			colorBuffer.GetNativeImage(), 
			vulkanContext.GetSwapChain().GetSwapChainImage(backBufferIndex), 
			colorBuffer.GetExtent(), 
			vulkanContext.GetSwapChain().GetSwapChainExtent());

		commandBuffer.TransitionImage(
			vulkanContext.GetSwapChain().GetSwapChainImage(backBufferIndex),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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

		auto graphicQueue = vulkanContext.GetDevice().GetNativeGraphicsQueue();

		if (vkQueueSubmit(graphicQueue, 1, &submitInfo, syncObject.GetInFlightFence()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}
		auto index = frameContext.GetBackBufferIndex();
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapChains[] = { vulkanContext.GetSwapChain().GetNativeSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &index;
		presentInfo.pResults = nullptr;

		auto presentcQueue = vulkanContext.GetDevice().GetNativePresentQueue();

		VkResult result = vkQueuePresentKHR(presentcQueue, &presentInfo);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || vulkanContext.GetWindowManager().IsWindowResized())
		{
			vulkanContext.GetWindowManager().ResetResizedState();
			vulkanContext.ResizeWindow();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image!");
		}
		threadContext.NextFrame();
	}

	void PresentPass::Cleanup(const RenderContext& context)
	{
	}

}



