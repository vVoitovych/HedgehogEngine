#include "RenderPass.hpp"
#include "VulkanWrappers/Device/Device.hpp"
#include "Logger/Logger.hpp"
#include "Common/EngineDebugBreak.hpp"

#include <array>

namespace Hedgehog
{
	namespace Wrappers
	{
		RenderPass::RenderPass(const Device& device, VkRenderPassCreateInfo* renderPassInfo)
			: mRenderPass(nullptr)
		{
			if (vkCreateRenderPass(device.GetNativeDevice(), renderPassInfo, nullptr, &mRenderPass) != VK_SUCCESS)
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

		void RenderPass::Cleanup(const Device& device)
		{
			vkDestroyRenderPass(device.GetNativeDevice(), mRenderPass, nullptr);
			mRenderPass = nullptr;
			LOGINFO("Render pass cleaned");
		}

		VkRenderPass RenderPass::GetNativeRenderPass() const
		{
			return mRenderPass;
		}
	}
}


