#include "RenderPass.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogCommon/Common/EngineDebugBreak.hpp"

#include "Logger/Logger.hpp"

#include <array>

namespace Wrappers
{

	RenderPass::RenderPass(const Device& device, VkRenderPassCreateInfo* renderPassInfo)
		: m_RenderPass(nullptr)
	{
		if (vkCreateRenderPass(device.GetNativeDevice(), renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create render pass");
		}
		LOGINFO("Render pass created");
	}

	RenderPass::~RenderPass()
	{
		if (m_RenderPass != nullptr)
		{
			LOGERROR("Vulkan rendere pass should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void RenderPass::Cleanup(const Device& device)
	{
		vkDestroyRenderPass(device.GetNativeDevice(), m_RenderPass, nullptr);
		m_RenderPass = nullptr;
		LOGINFO("Render pass cleaned");
	}

	VkRenderPass RenderPass::GetNativeRenderPass() const
	{
		return m_RenderPass;
	}

}


