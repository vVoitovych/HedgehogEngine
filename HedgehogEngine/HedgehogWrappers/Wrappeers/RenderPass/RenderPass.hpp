#pragma once

#include <vulkan/vulkan.h>

namespace Wrappers
{
	class Device;

	class RenderPass
	{
	public:
		RenderPass(const Device& device, VkRenderPassCreateInfo* renderPassInfo);
		~RenderPass();

		RenderPass(const RenderPass&) = delete;
		RenderPass& operator=(const RenderPass&) = delete;
		RenderPass(RenderPass&&) = delete;
		RenderPass& operator=(RenderPass&&) = delete;

		void Cleanup(const Device& device);

		VkRenderPass GetNativeRenderPass() const;
	private:
		VkRenderPass m_RenderPass;

	};
}



