#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	class Device;

	class DepthBuffer
	{
	public:
		DepthBuffer();
		~DepthBuffer();

		DepthBuffer(const DepthBuffer&) = delete;
		DepthBuffer& operator=(const DepthBuffer&) = delete;

		void Initialize(const Device& device, VkExtent2D extend);
		void Cleanup(const Device& device);

		VkImageView GetNativeView() const;

	private:
		VkImage mDepthImage;
		VkDeviceMemory mDepthImageMemory;
		VkImageView mDepthImageView;

	};

}

