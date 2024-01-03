#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;

	class RenderPass
	{
	public:
		RenderPass(const std::unique_ptr<Device>& device, VkRenderPassCreateInfo* renderPassInfo);
		~RenderPass();

		RenderPass(const RenderPass&) = delete;
		RenderPass& operator=(const RenderPass&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkRenderPass GetNativeRenderPass() const;
	private:
		VkRenderPass mRenderPass;

	};
}



