#include "InitPass.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/RendererContext.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"
#include "HedgehogWrappers/Wrappeers/Commands/CommandBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/SyncObjects/SyncObject.hpp"

#include <stdexcept>

namespace Renderer
{
	InitPass::InitPass(const Context::Context& context)
	{
	}

	void InitPass::Render(Context::Context& context)
	{
		auto& vulkanContext = context.GetVulkanContext();
		auto& rendererContext = context.GetRendererContext();

		auto& syncObject = rendererContext.GetSyncObject();
		syncObject.WaitforInFlightFence(vulkanContext.GetDevice());

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(
			vulkanContext.GetDevice().GetNativeDevice(), 
			vulkanContext.GetSwapChain().GetNativeSwapChain(),
			UINT64_MAX, 
			syncObject.GetImageAvailableSemaphore(), 
			VK_NULL_HANDLE, 
			&imageIndex);
		rendererContext.SetBackBufferIndex(imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			vulkanContext.ResizeWindow();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		syncObject.ResetInFlightFence(vulkanContext.GetDevice());
		auto& commandBuffer = rendererContext.GetCommandBuffer();
		commandBuffer.BeginCommandBuffer(0);
	}

	void InitPass::Cleanup(const Context::Context& context)
	{

	}



}

