#include "CommandPool.h"
#include "VulkanEngine/Renderer/Device/Device.h"

namespace Renderer
{
	CommandPool::CommandPool()
		: mCommandPool(nullptr)
		, mDevice(nullptr)
	{
	}

	CommandPool::~CommandPool()
	{
		if (mCommandPool != nullptr)
		{
			throw std::runtime_error("Vulkan command pool should be cleanedup before destruction!");
		}
	}

	void CommandPool::Initialize(Device& device)
	{
		mDevice = device.GetNativeDevice();
		QueueFamilyIndices indicies = device.GetIndicies();

		VkCommandPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		createInfo.queueFamilyIndex = indicies.mGraphicsFamily.value();

		if (vkCreateCommandPool(mDevice, &createInfo, nullptr, &mCommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool!");
		}

		std::cout << "Command pool created" << std::endl;
	}

	void CommandPool::Cleanup()
	{
		vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
		mCommandPool = nullptr;
		std::cout << "Command pool cleaned" << std::endl;
	}

	VkCommandPool CommandPool::GetNativeCommandPool()
	{
		return mCommandPool;
	}

}


