#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	class Device;

	class RenderPass
	{
	public:
		RenderPass();
		~RenderPass();

		RenderPass(const RenderPass&) = delete;
		RenderPass& operator=(const RenderPass&) = delete;

		void Initialize(const Device& device, VkFormat format);
		void Cleanup(const Device& device);

		VkRenderPass GetNativeRenderPass() const;
	private:
		VkRenderPass mRenderPass;

	};
}



