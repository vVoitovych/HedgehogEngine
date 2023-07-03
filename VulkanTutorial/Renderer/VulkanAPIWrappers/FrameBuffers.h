#pragma once

#include "../Common/pch.h"
#include "Device.h"
#include "SwapChain.h"
#include "RenderPass.h"

namespace Renderer
{
	class FrameBuffers
	{
	public:
		FrameBuffers();
		~FrameBuffers();

		FrameBuffers(const FrameBuffers&) = delete;
		FrameBuffers& operator=(const FrameBuffers&) = delete;

		void Initialize(Device& device, SwapChain& swapChain, RenderPass& renderPass);
		void Cleanup(Device& device);

		VkFramebuffer GetFrameBuffer(size_t index) const;

	private:
		std::vector<VkFramebuffer> mFrameBuffers;
	};
}

