#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace Renderer
{
	class Device;
	class DepthBuffer;
	class RenderPass;

	class FrameBuffer
	{
	public:
		FrameBuffer(
			const std::unique_ptr<Device>& device, 
			std::vector<VkImageView> attachments, 
			VkExtent2D extent, 
			const std::unique_ptr<RenderPass>& renderPass);
		~FrameBuffer();

		FrameBuffer(const FrameBuffer&) = delete;
		FrameBuffer& operator=(const FrameBuffer&) = delete;

		FrameBuffer(FrameBuffer&& other) noexcept;
		FrameBuffer& operator=(FrameBuffer&& other) noexcept;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkFramebuffer GetNativeFrameBuffer() const;

	private:
		VkFramebuffer mFrameBuffer;
	};
}

