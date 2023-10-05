#include "CommandPool.h"
#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.h"

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
			LOGERROR("Vulkan command pool should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
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

		LOGINFO("Command pool created");
	}

	void CommandPool::Cleanup()
	{
		vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
		mCommandPool = nullptr;
		LOGINFO("Command pool cleaned");
	}

	VkCommandPool CommandPool::GetNativeCommandPool()
	{
		return mCommandPool;
	}

}


