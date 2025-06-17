#include "RenderQueue.hpp"

#include "HedgehogContext/Context/Context.hpp"
#include "HedgehogContext/Context/VulkanContext.hpp"
#include "HedgehogRenderer/ResourceManager/ResourceManager.hpp"
#include "HedgehogRenderer/RenderPasses/InitPass/InitPass.hpp"
#include "HedgehogRenderer/RenderPasses/DepthPrepass/DepthPrePass.hpp"
#include "HedgehogRenderer/RenderPasses/ShadowmapPass/ShadowmapPass.hpp"
#include "HedgehogRenderer/RenderPasses/ForwardPass/ForwardPass.hpp"
#include "HedgehogRenderer/RenderPasses/PresentPass/PresentPass.hpp"
#include "HedgehogRenderer/RenderPasses/GuiPass/GuiPass.hpp"

namespace Renderer
{
	RenderQueue::RenderQueue(const Context::Context& context, const ResourceManager& resourceManager)
	{
		m_InitPass = std::make_unique<InitPass>(context);
		m_DepthPrePass = std::make_unique<DepthPrePass>(context, resourceManager);
		m_ShadowmapPass = std::make_unique<ShadowmapPass>(context, resourceManager);
		m_ForwardPass = std::make_unique<ForwardPass>(context, resourceManager);
		m_GuiPass = std::make_unique<GuiPass>(context, resourceManager);
		m_PresentPass = std::make_unique<PresentPass>(context);
	}

	RenderQueue::~RenderQueue()
	{
	}

	void RenderQueue::Cleanup(const Context::Context& context)
	{
		m_InitPass->Cleanup(context);
		m_DepthPrePass->Cleanup(context);
		m_ShadowmapPass->Cleanup(context);
		m_ForwardPass->Cleanup(context);
		m_GuiPass->Cleanup(context);
		m_PresentPass->Cleanup(context);
	}

	void RenderQueue::Render(Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();
		if (vulkanContext.IsWindowResized())
			return;

		m_InitPass->Render(context);
		m_ShadowmapPass->Render(context, resourceManager);
		m_DepthPrePass->Render(context, resourceManager);
		m_ForwardPass->Render(context, resourceManager);
		m_GuiPass->Render(context, resourceManager);
		m_PresentPass->Render(context, resourceManager);

	}

	void RenderQueue::UpdateData(const Context::Context& context)
	{
		m_ShadowmapPass->UpdateData(context);
	}

	void RenderQueue::ResizeResources(const Context::Context& context, const ResourceManager& resourceManager)
	{
		m_DepthPrePass->ResizeResources(context, resourceManager);
		m_ForwardPass->ResizeResources(context, resourceManager);
		m_GuiPass->ResizeResources(context, resourceManager);
	}

	void RenderQueue::UpdateResources(const Context::Context& context, const ResourceManager& resourceManager)
	{
		m_ShadowmapPass->UpdateResources(context, resourceManager);
	}



}




