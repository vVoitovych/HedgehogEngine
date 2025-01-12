#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Wrappers
{
	class Device;
	class RenderPass;

	class FrameBuffer
	{
	public:
		FrameBuffer(
			const Device& device, 
			std::vector<VkImageView> attachments, 
			VkExtent2D extent, 
			const RenderPass& renderPass);
		~FrameBuffer();

		FrameBuffer(const FrameBuffer&) = delete;
		FrameBuffer& operator=(const FrameBuffer&) = delete;

		FrameBuffer(FrameBuffer&& other) noexcept;
		FrameBuffer& operator=(FrameBuffer&& other) noexcept;

		void Cleanup(const Device& device);

		VkFramebuffer GetNativeFrameBuffer() const;

	private:
		VkFramebuffer mFrameBuffer;
	};
}

