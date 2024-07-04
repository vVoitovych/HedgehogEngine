#include "InitPass.hpp"
#include "Context/VulkanContext.hpp"
#include "Context/Context.hpp"
#include "Context/ThreadContext.hpp"
#include "Context/EngineContext.hpp"
#include "Context/FrameContext.hpp"

#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/SwapChain/SwapChain.hpp"
#include "Wrappeers/Commands/CommandBuffer.hpp"
#include "Wrappeers/SyncObjects/SyncObject.hpp"

#include <stdexcept>

namespace Renderer
{
	InitPass::InitPass(const Context::Context& context)
	{
	}

	void InitPass::Render(Context::Context& context)
	{
		auto& vulkanContext = context.GetVulkanContext();
		auto& frameContext = context.GetFrameContext();
		auto& threadContext = context.GetThreadContext();

		auto& syncObject = threadContext.GetSyncObject();
		syncObject.WaitforInFlightFence(vulkanContext.GetDevice());

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(
			vulkanContext.GetDevice().GetNativeDevice(), 
			vulkanContext.GetSwapChain().GetNativeSwapChain(),
			UINT64_MAX, 
			syncObject.GetImageAvailableSemaphore(), 
			VK_NULL_HANDLE, 
			&imageIndex);
		frameContext.SetBackBufferIndex(imageIndex);

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
		auto& commandBuffer = threadContext.GetCommandBuffer();
		commandBuffer.BeginCommandBuffer(0);
	}

	void InitPass::Cleanup(const Context::Context& context)
	{

	}



}

