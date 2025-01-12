#include "Renderer.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogContext/Context/FrameContext.hpp"
#include "HedgehogContext/Context/ThreadContext.hpp"
#include "HedgehogRenderer/RenderQueue/RenderQueue.hpp"
#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	Renderer::Renderer(const Context::Context& context)
	{
		mResourceManager = std::make_unique<ResourceManager>(context);
		mRenderQueue = std::make_unique<RenderQueue>(context, *mResourceManager);

	}

	Renderer::~Renderer()
	{
	}
	 
	void Renderer::Cleanup(const Context::Context& context)
	{
		vkQueueWaitIdle(context.GetVulkanContext().GetDevice().GetNativeGraphicsQueue());

		mRenderQueue->Cleanup(context);
		mResourceManager->Cleanup(context);
	}

	void Renderer::DrawFrame(Context::Context& context)
	{		
		auto& vulkanContext = context.GetVulkanContext();
		if (vulkanContext.IsWindowResized())
		{
			RecreateSwapChain(context);
			vulkanContext.ResetWindowResizeState();
		}

		mRenderQueue->Render(context, *mResourceManager);
	}

	void Renderer::RecreateSwapChain(Context::Context& context)
	{
		vkDeviceWaitIdle(context.GetVulkanContext().GetDevice().GetNativeDevice());
		auto& vulkanContext = context.GetVulkanContext();
		vulkanContext.GetSwapChain().Recreate(vulkanContext.GetDevice());

		mResourceManager->ResizeResources(context);
		mRenderQueue->ResizeResources(context, *mResourceManager);
	}

}



