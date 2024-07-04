#include "ResourceManager.hpp"

#include "Context/Context.hpp"
#include "Context/EngineContext.hpp"
#include "Context/ThreadContext.hpp"
#include "Context/VulkanContext.hpp"
#include "Context/FrameContext.hpp"

#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/SwapChain/SwapChain.hpp"
#include "Wrappeers/Resources/Image/Image.hpp"
#include "Wrappeers/FrameBuffer/FrameBuffer.hpp"

#include "Common/RendererSettings.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	ResourceManager::ResourceManager(const Context::Context& context)
	{
		CreateColorBuffer(context);
		CreateDepthBuffer(context);
	}

	ResourceManager::~ResourceManager()
	{
	}

	void ResourceManager::Cleanup(const Context::Context& context)
	{
		auto& vulkanContext = context.GetVulkanContext();
		mColorBuffer->Cleanup(vulkanContext.GetDevice());
		mDepthBuffer->Cleanup(vulkanContext.GetDevice());
	}

	void ResourceManager::ResizeResources(const Context::Context& context)
	{
		auto& vulkanContext = context.GetVulkanContext();
		mColorBuffer->Cleanup(vulkanContext.GetDevice());
		mDepthBuffer->Cleanup(vulkanContext.GetDevice());

		CreateColorBuffer(context);
		CreateDepthBuffer(context);
	}

	const Wrappers::Image& ResourceManager::GetColorBuffer() const
	{
		return *mColorBuffer;
	}

	const Wrappers::Image& ResourceManager::GetDepthBuffer() const
	{
		return *mDepthBuffer;
	}

	void ResourceManager::CreateDepthBuffer(const Context::Context& context)
	{
		auto& vulkanContext = context.GetVulkanContext();
		auto depthFormat = vulkanContext.GetDevice().FindDepthFormat();
		auto extend = vulkanContext.GetSwapChain().GetSwapChainExtent();

		mDepthBuffer = std::make_unique<Wrappers::Image>(
			vulkanContext.GetDevice(),
			extend.width,
			extend.height,
			depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		mDepthBuffer->CreateImageView(vulkanContext.GetDevice(), depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
		vulkanContext.GetDevice().SetObjectName(reinterpret_cast<uint64_t>(mDepthBuffer->GetNativeImage()), VK_OBJECT_TYPE_IMAGE, "DepthBuffer");
		LOGINFO("Depth buffer created");
	}

	void ResourceManager::CreateColorBuffer(const Context::Context& context)
	{
		auto& vulkanContext = context.GetVulkanContext();
		auto colorFormat = VK_FORMAT_R16G16B16A16_UNORM;
		auto extend = vulkanContext.GetSwapChain().GetSwapChainExtent();

		mColorBuffer = std::make_unique<Wrappers::Image>(
			vulkanContext.GetDevice(),
			extend.width,
			extend.height,
			colorFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		mColorBuffer->CreateImageView(vulkanContext.GetDevice(), colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		vulkanContext.GetDevice().SetObjectName(reinterpret_cast<uint64_t>(mColorBuffer->GetNativeImage()), VK_OBJECT_TYPE_IMAGE, "ColorBuffer");
		LOGINFO("Color buffer created");
	}

}

