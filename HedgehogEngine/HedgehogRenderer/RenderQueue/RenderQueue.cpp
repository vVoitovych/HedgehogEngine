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

#include "HedgehogEngine/HedgehogCommon/CpuProfiler/CpuProfiler.hpp"


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

		START_TIME_STAMP("Init pass");
		m_InitPass->Render(context);
		END_TIME_STAMP("Init pass");

		START_TIME_STAMP("Shadowmap pass");
		m_ShadowmapPass->Render(context, resourceManager);
		END_TIME_STAMP("Shadowmap pass");

		START_TIME_STAMP("Depth pre pass");
		m_DepthPrePass->Render(context, resourceManager);
		END_TIME_STAMP("Depth pre pass");

		START_TIME_STAMP("Forward pass");
		m_ForwardPass->Render(context, resourceManager);
		END_TIME_STAMP("Forward pass");

		START_TIME_STAMP("GUI pass");
		m_GuiPass->Render(context, resourceManager);
		END_TIME_STAMP("GUI pass");

		START_TIME_STAMP("Present pass");
		m_PresentPass->Render(context, resourceManager);
		END_TIME_STAMP("Present pass");

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




