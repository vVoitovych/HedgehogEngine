#include "TextureImageView.h"

#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Renderer/TextureImage/TextureImage.h"

#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.h"

namespace Renderer
{
	TextureImageView::TextureImageView()
		: mTextureImageView(nullptr)
	{
	}

	TextureImageView::~TextureImageView()
	{
		if (mTextureImageView != nullptr)
		{
			LOGERROR("Vulkan texture image view should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void TextureImageView::Initialize(const Device& device, const TextureImage& textureImage, VkFormat format)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = textureImage.GetNativeImage();
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device.GetNativeDevice(), &viewInfo, nullptr, &mTextureImageView) != VK_SUCCESS) 
		{
			throw std::runtime_error("failed to create texture image view!");
		}
		LOGINFO("Vulkan texture image view created");
	}

	void TextureImageView::Cleanup(const Device& device)
	{
		vkDestroyImageView(device.GetNativeDevice(), mTextureImageView, nullptr);
		mTextureImageView = nullptr;
		LOGINFO("Vulkan texture image view cleaned");
	}
	VkImageView TextureImageView::GetNativeImageView() const
	{
		return mTextureImageView;
	}
}



