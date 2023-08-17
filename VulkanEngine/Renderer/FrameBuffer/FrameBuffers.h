#pragma once

#include "VulkanEngine/Renderer/Common/pch.h"

namespace Renderer
{
	class Device;
	class SwapChain;
	class RenderPass;

	class FrameBuffers
	{
	public:
		FrameBuffers();
		~FrameBuffers();

		FrameBuffers(const FrameBuffers&) = delete;
		FrameBuffers& operator=(const FrameBuffers&) = delete;

		void Initialize(Device& device, SwapChain& swapChain, RenderPass& renderPass);
		void Cleanup();

		VkFramebuffer GetNativeFrameBuffer(size_t index) const;

	private:
		VkDevice mDevice;
		std::vector<VkFramebuffer> mFrameBuffers;
	};
}

