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
		m_ResourceManager = std::make_unique<ResourceManager>(context);
		m_RenderQueue = std::make_unique<RenderQueue>(context, *m_ResourceManager);

	}

	Renderer::~Renderer()
	{
	}
	 
	void Renderer::Cleanup(const Context::Context& context)
	{
		vkQueueWaitIdle(context.GetVulkanContext().GetDevice().GetNativeGraphicsQueue());

		m_RenderQueue->Cleanup(context);
		m_ResourceManager->Cleanup(context);
	}

	void Renderer::DrawFrame(Context::Context& context)
	{		
		auto& vulkanContext = context.GetVulkanContext();
		if (vulkanContext.IsWindowResized())
		{
			RecreateSwapChain(context);
			vulkanContext.ResetWindowResizeState();
		}

		m_RenderQueue->Render(context, *m_ResourceManager);
	}

	void Renderer::RecreateSwapChain(Context::Context& context)
	{
		vkDeviceWaitIdle(context.GetVulkanContext().GetDevice().GetNativeDevice());
		auto& vulkanContext = context.GetVulkanContext();
		vulkanContext.GetSwapChain().Recreate(vulkanContext.GetDevice());

		m_ResourceManager->ResizeResources(context);
		m_RenderQueue->ResizeResources(context, *m_ResourceManager);
	}

}



