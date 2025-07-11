#include "PresentPass.hpp"

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"
#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/RendererContext.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"

#include "HedgehogWrappers/WindowManagment/WindowManager.hpp"
#include "HedgehogWrappers/Wrappeers/Commands/CommandBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/SyncObjects/SyncObject.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Image/Image.hpp"

#include <stdexcept>

namespace Renderer
{
	PresentPass::PresentPass(const Context::Context& context)
	{
	}

	void PresentPass::Render(Context::Context& context, const ResourceManager& resourceManager)
	{

		auto& engineContext = context.GetEngineContext();
		auto& rendererContext = context.GetRendererContext();
		auto& vulkanContext = context.GetVulkanContext();

		auto& syncObject = rendererContext.GetSyncObject();
		auto& commandBuffer = rendererContext.GetCommandBuffer();

		auto backBufferIndex = rendererContext.GetBackBufferIndex();
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
		auto index = rendererContext.GetBackBufferIndex();
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
		rendererContext.NextFrame();
	}

	void PresentPass::Cleanup(const Context::Context& context)
	{
	}

}



