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

		void Cleanup(const Device& device);

		VkRenderPass GetNativeRenderPass() const;
	private:
		VkRenderPass mRenderPass;

	};
}



