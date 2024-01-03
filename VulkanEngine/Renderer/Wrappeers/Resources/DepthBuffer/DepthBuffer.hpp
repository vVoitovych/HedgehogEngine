#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;

	class DepthBuffer
	{
	public:
		DepthBuffer(const std::unique_ptr<Device>& device, VkExtent2D extend);
		~DepthBuffer();

		DepthBuffer(const DepthBuffer&) = delete;
		DepthBuffer& operator=(const DepthBuffer&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkImageView GetNativeView() const;

	private:
		VkImage mDepthImage;
		VkDeviceMemory mDepthImageMemory;
		VkImageView mDepthImageView;

	};

}

