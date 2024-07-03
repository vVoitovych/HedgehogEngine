#include "Renderer.hpp"

#include "Context/RenderContext.hpp"
#include "Context/VulkanContext.hpp"
#include "Context/EngineContext.hpp"
#include "Context/FrameContext.hpp"
#include "Context/ThreadContext.hpp"
#include "RenderQueue/RenderQueue.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/SwapChain/SwapChain.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	Renderer::Renderer(const RenderContext& context)
	{
		mResourceManager = std::make_unique<ResourceManager>(context);
		mRenderQueue = std::make_unique<RenderQueue>(context, *mResourceManager);

	}

	Renderer::~Renderer()
	{
	}
	 
	void Renderer::Cleanup(const RenderContext& context)
	{
		vkQueueWaitIdle(context.GetVulkanContext().GetDevice().GetNativeGraphicsQueue());

		mRenderQueue->Cleanup(context);
		mResourceManager->Cleanup(context);
	}

	void Renderer::DrawFrame(RenderContext& context)
	{		
		auto& vulkanContext = context.GetVulkanContext();
		if (vulkanContext.IsWindowResized())
		{
			RecreateSwapChain(context);
			vulkanContext.ResetWindowResizeState();
		}

		mRenderQueue->Render(context, *mResourceManager);
	}

	void Renderer::RecreateSwapChain(RenderContext& context)
	{
		vkDeviceWaitIdle(context.GetVulkanContext().GetDevice().GetNativeDevice());
		auto& vulkanContext = context.GetVulkanContext();
		vulkanContext.GetSwapChain().Recreate(vulkanContext.GetDevice());

		mResourceManager->ResizeResources(context);
		mRenderQueue->ResizeResources(context, *mResourceManager);
	}

}



