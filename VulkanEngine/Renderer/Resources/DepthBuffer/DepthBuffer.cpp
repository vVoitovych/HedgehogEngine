#include "DepthBuffer.h"

#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.h"

namespace Renderer
{
	DepthBuffer::DepthBuffer()
		: mDepthImage(nullptr)
		, mDepthImageMemory(nullptr)
		, mDepthImageView(nullptr)
	{
	}

	DepthBuffer::~DepthBuffer()
	{
		if (mDepthImage != nullptr)
		{
			LOGERROR("Vulkan depth buffer image should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mDepthImageMemory != nullptr)
		{
			LOGERROR("Vulkan depth buffer memory should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mDepthImageView != nullptr)
		{
			LOGERROR("Vulkan depth buffer view should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void DepthBuffer::Initialize(const Device& device, VkExtent2D extend)
	{
		VkFormat depthFormat = device.FindDepthFormat();

		device.CreateImage(extend.width, extend.height, depthFormat,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			mDepthImage, mDepthImageMemory);

		mDepthImageView = device.CreateImageView(mDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	void DepthBuffer::Cleanup(const Device& device)
	{
		device.DestroyImage(mDepthImage, nullptr);
		device.FreeMemory(mDepthImageMemory, nullptr);
		vkDestroyImageView(device.GetNativeDevice(), mDepthImageView, nullptr);

		mDepthImage = nullptr;
		mDepthImageMemory = nullptr;
		mDepthImageView = nullptr;

		LOGINFO("Vulkan depth buffer cleaned");
	}

	VkImageView DepthBuffer::GetNativeView() const
	{
		return mDepthImageView;
	}






}



