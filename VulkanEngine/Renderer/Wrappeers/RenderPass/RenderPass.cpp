#include "RenderPass.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Logger/Logger.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"

#include <array>

namespace Renderer
{

	RenderPass::RenderPass(const std::unique_ptr<Device>& device, VkRenderPassCreateInfo* renderPassInfo)
		: mRenderPass(nullptr)
	{
		if (vkCreateRenderPass(device->GetNativeDevice(), renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create render pass");
		}
		LOGINFO("Render pass created");
	}

	RenderPass::~RenderPass()
	{
		if (mRenderPass != nullptr)
		{
			LOGERROR("Vulkan rendere pass should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void RenderPass::Cleanup(const std::unique_ptr<Device>& device)
	{
		vkDestroyRenderPass(device->GetNativeDevice(), mRenderPass, nullptr);
		mRenderPass = nullptr;
		LOGINFO("Render pass cleaned");
	}

	VkRenderPass RenderPass::GetNativeRenderPass() const
	{
		return mRenderPass;
	}

}


