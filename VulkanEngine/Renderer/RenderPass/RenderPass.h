#pragma once

#include "VulkanEngine/Renderer/Common/pch.h"

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

		void Initialize(Device& device, VkFormat format);
		void Cleanup();

		VkRenderPass GetNativeRenderPass() const;
	private:
		VkDevice mDevice;
		VkRenderPass mRenderPass;

	};
}



