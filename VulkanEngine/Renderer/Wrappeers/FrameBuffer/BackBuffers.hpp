#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Renderer
{
	class Device;
	class SwapChain;
	class DepthBuffer;
	class RenderPass;

	class BackBuffers
	{
	public:
		BackBuffers();
		~BackBuffers();

		BackBuffers(const BackBuffers&) = delete;
		BackBuffers& operator=(const BackBuffers&) = delete;

		void Initialize(const Device& device, SwapChain& swapChain, const DepthBuffer& depthBuffer, RenderPass& renderPass);
		void Cleanup(const Device& device);

		VkFramebuffer GetNativeFrameBuffer(size_t index) const;

	private:
		std::vector<VkFramebuffer> mBackBuffers;
	};
}

