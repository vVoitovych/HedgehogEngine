#include "DescriptorPool.h"
#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Logger/Logger.h"

namespace Renderer
{
	DescriptorPool::DescriptorPool()
		: mDevice(nullptr)
		, mDescriptorPool(nullptr)
	{
	}

	DescriptorPool::~DescriptorPool()
	{
		if (mDescriptorPool != nullptr)
		{
			LOGERROR("Vulkan description pool should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void DescriptorPool::Initialize(Device& device)
	{
		mDevice = device.GetNativeDevice();

		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

		if (vkCreateDescriptorPool(mDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor pool!");
		}
	}

	void DescriptorPool::Cleanup()
	{
		vkDestroyDescriptorPool(mDevice, mDescriptorPool, nullptr);
		mDescriptorPool = nullptr;
	}

	VkDescriptorPool DescriptorPool::GetNativePool()
	{
		return mDescriptorPool;
	}

}

