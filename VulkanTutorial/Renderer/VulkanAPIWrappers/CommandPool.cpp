#include "CommandPool.h"

namespace Renderer
{
	CommandPool::CommandPool()
		: mCommandPool(VK_NULL_HANDLE)
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
		QueueFamilyIndices indicies = device.GetIndicies();

		VkCommandPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		createInfo.queueFamilyIndex = indicies.mGraphicsFamily.value();

		if (vkCreateCommandPool(device.GetDevice(), &createInfo, nullptr, &mCommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool!");
		}

		std::cout << "Command pool created" << std::endl;
	}

	void CommandPool::Cleanup(Device& device)
	{
		vkDestroyCommandPool(device.GetDevice(), mCommandPool, nullptr);
		mCommandPool = nullptr;
	}

	VkCommandPool CommandPool::GetCommandPool()
	{
		return mCommandPool;
	}

}


