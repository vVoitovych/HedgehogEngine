#include "CommandPool.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Logger/Logger.hpp"

namespace Renderer
{
	CommandPool::CommandPool(const std::unique_ptr<Device>& device)
		: mCommandPool(nullptr)
	{
		QueueFamilyIndices indicies = device->GetIndicies();

		VkCommandPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		createInfo.queueFamilyIndex = indicies.mGraphicsFamily.value();

		if (vkCreateCommandPool(device->GetNativeDevice(), &createInfo, nullptr, &mCommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool!");
		}

		LOGINFO("Command pool created");
	}

	CommandPool::~CommandPool()
	{
		if (mCommandPool != nullptr)
		{
			LOGERROR("Vulkan command pool should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void CommandPool::Cleanup(const std::unique_ptr<Device>& device)
	{
		vkDestroyCommandPool(device->GetNativeDevice(), mCommandPool, nullptr);
		mCommandPool = nullptr;
	}

	const VkCommandPool& CommandPool::GetNativeCommandPool() const
	{
		return mCommandPool;
	}

	void CommandPool::AllocateCommandBuffer(const std::unique_ptr<Device>& device, VkCommandBuffer* pCommandBuffer) const
	{
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = mCommandPool;
		allocateInfo.commandBufferCount = 1;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		if (vkAllocateCommandBuffers(device->GetNativeDevice(), &allocateInfo, pCommandBuffer) != VK_SUCCESS)
		{
			LOGERROR("failed to allocate command buffer!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void CommandPool::FreeCommandBuffer(const std::unique_ptr<Device>& device, VkCommandBuffer* pCommandBuffer) const
	{
		vkFreeCommandBuffers(device->GetNativeDevice(), mCommandPool, 1, pCommandBuffer);
	}

}



