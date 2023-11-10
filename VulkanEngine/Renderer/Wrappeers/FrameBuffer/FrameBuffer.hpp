#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Renderer
{
	class Device;
	class SwapChain;
	class DepthBuffer;
	class RenderPass;

	class FrameBuffer
	{
	public:
		FrameBuffer();
		~FrameBuffer();

		FrameBuffer(const FrameBuffer&) = delete;
		FrameBuffer& operator=(const FrameBuffer&) = delete;

		void Initialize(const Device& device, std::vector<VkImageView> attachments, VkExtent2D extent, RenderPass& renderPass);
		void Cleanup(const Device& device);

		VkFramebuffer GetNativeFrameBuffer() const;

	private:
		VkFramebuffer mFrameBuffer;
	};
}

