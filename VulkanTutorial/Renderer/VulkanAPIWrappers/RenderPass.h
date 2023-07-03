#pragma once

#include "../Common/pch.h"
#include "Device.h"

namespace Renderer
{
	class RenderPass
	{
	public:
		RenderPass();
		~RenderPass();

		RenderPass(const RenderPass&) = delete;
		RenderPass& operator=(const RenderPass&) = delete;

		void Initialize(Device& device, VkFormat format);
		void Cleanup(Device& device);

		VkRenderPass GetRenderPass();
	private:
		VkRenderPass mRenderPass;

	};
}



