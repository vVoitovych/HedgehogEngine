#include "VulkanContext.hpp"

#include "Renderer/WindowManagment/WindowManager.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "Renderer/Wrappeers/Commands/CommandPool.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorPool.hpp"
#include "Renderer/Common/RendererSettings.hpp"

#include <vector>

namespace Renderer
{
	VulkanContext::VulkanContext()
	{
		mWindowManager = std::make_unique<WindowManager>(WindowState::GetDefaultState());
		mDevice = std::make_unique<Device>(mWindowManager);
		mSwapChain = std::make_unique<SwapChain>(mDevice, mWindowManager);

		mCommandPool = std::make_unique<CommandPool>(mDevice);

		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.resize(2);

		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		mDescriptorPool = std::make_unique<DescriptorPool>(mDevice, poolSizes, 2);
	}

	VulkanContext::~VulkanContext()
	{
	}

	void VulkanContext::Cleanup()
	{
		mDescriptorPool->Cleanup(mDevice);
		mCommandPool->Cleanup();
		mSwapChain->Cleanup(mDevice);
		mDevice->Cleanup();
	}

	void VulkanContext::HandleInput()
	{
		mWindowManager->HandleInput();
	}

	const std::unique_ptr<WindowManager>& VulkanContext::GetWindowManager() const
	{
		return mWindowManager;
	}

	const std::unique_ptr<Device>& VulkanContext::GetDevice() const
	{
		return mDevice;
	}

	const std::unique_ptr<SwapChain>& VulkanContext::GetSwapChain() const
	{
		return mSwapChain;
	}

	const std::unique_ptr<CommandPool>& VulkanContext::GetCommandPool() const
	{
		return mCommandPool;
	}

	const std::unique_ptr<DescriptorPool>& VulkanContext::GetDescriptorPool() const
	{
		return mDescriptorPool;
	}

	bool VulkanContext::ShouldClose() const
	{
		return mWindowManager->ShouldClose();
	}

	void VulkanContext::ResizeWindow()
	{
		mWindowResized = true;
	}

	bool VulkanContext::IsWindowResized()
	{
		return mWindowResized;
	}

	void VulkanContext::ResetWindowResizeState()
	{
		mWindowResized = false;
	}

}

