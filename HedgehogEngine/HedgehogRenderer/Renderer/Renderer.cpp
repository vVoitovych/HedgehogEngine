#include "Renderer.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogContext/Context/EngineContext.hpp"
#include "HedgehogRenderer/RenderQueue/RenderQueue.hpp"
#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"
#include "HedgehogSettings/Settings/HedgehogSettings.hpp"

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
		auto& settings = context.GetEngineContext().GetSettings();
		m_RenderQueue->UpdateData(context);
		if (vulkanContext.IsWindowResized())
		{
			vkDeviceWaitIdle(vulkanContext.GetDevice().GetNativeDevice());
			vulkanContext.GetSwapChain().Recreate(vulkanContext.GetDevice());

			m_ResourceManager->ResizeFrameBufferSizeDependenteResources(context);
			m_RenderQueue->ResizeResources(context, *m_ResourceManager);

			vulkanContext.ResetWindowResizeState();
		}
		if (settings.IsDirty())
		{
			m_ResourceManager->ResizeSettingsDependenteResources(context);
			m_RenderQueue->UpdateResources(context, *m_ResourceManager);

			settings.CleanDirtyState();
		}

		m_RenderQueue->Render(context, *m_ResourceManager);
	}

}



