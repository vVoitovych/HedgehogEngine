#include "CommandPool.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Logger/Logger.hpp"

namespace Renderer
{
	CommandPool::CommandPool(const std::unique_ptr<Device>& device)
		: mDevice(nullptr)
		, mQueue(nullptr)
		, mCommandPool(nullptr)
	{
		QueueFamilyIndices indicies = device->GetIndicies();
		mDevice = device->GetNativeDevice();
		mQueue = device->GetNativeGraphicsQueue();

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

	CommandPool::~CommandPool()
	{
		if (mCommandPool != nullptr)
		{
			LOGERROR("Vulkan command pool should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void CommandPool::Cleanup()
	{
		vkDestroyCommandPool(mDevice, mCommandPool, nullptr);
		mCommandPool = nullptr;
		mDevice = nullptr;
		mQueue = nullptr;
	}

	const VkCommandPool& CommandPool::GetNativeCommandPool() const
	{
		return mCommandPool;
	}
	VkCommandPool CommandPool::GetNativeCommandPool()
	{
		return mCommandPool;
	}

	void CommandPool::AllocateCommandBuffer(VkCommandBuffer* pCommandBuffer) const
	{
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = mCommandPool;
		allocateInfo.commandBufferCount = 1;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		if (vkAllocateCommandBuffers(mDevice, &allocateInfo, pCommandBuffer) != VK_SUCCESS)
		{
			LOGERROR("failed to allocate command buffer!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void CommandPool::FreeCommandBuffer(VkCommandBuffer* pCommandBuffer) const
	{
		vkFreeCommandBuffers(mDevice, mCommandPool, 1, pCommandBuffer);
	}

	VkCommandBuffer CommandPool::BeginSingleTimeCommands() const
	{
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = mCommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(mDevice, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void CommandPool::EndSingleTimeCommands(VkCommandBuffer commandBuffer) const
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(mQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(mQueue);

		vkFreeCommandBuffers(mDevice, mCommandPool, 1, &commandBuffer);
	}



}



