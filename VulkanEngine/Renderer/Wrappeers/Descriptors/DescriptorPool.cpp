#include "DescriptorPool.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorSetLayout.hpp"

#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"

#include <stdexcept>

namespace Renderer
{
	DescriptorPool::DescriptorPool(const std::unique_ptr<Device>& device, const std::vector<VkDescriptorPoolSize> poolSizes, const uint32_t descriptorSetsCount)
		: mDescriptorPool(nullptr) 
	{
		if (poolSizes.size() == 0)
		{
			throw std::runtime_error("Failed to create descriptor pool!");
		}

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = descriptorSetsCount;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

		if (vkCreateDescriptorPool(device->GetNativeDevice(), &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create descriptor pool!");
		}
		LOGINFO("Descriptor pool cleaned");
	}

	DescriptorPool::~DescriptorPool()
	{
		if (mDescriptorPool != nullptr)
		{
			LOGERROR("Vulkan descriptor pool should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void DescriptorPool::Cleanup(const std::unique_ptr<Device>& device)
	{
		vkDestroyDescriptorPool(device->GetNativeDevice(), mDescriptorPool, nullptr);
		mDescriptorPool = nullptr;
	}

	const VkDescriptorPool& DescriptorPool::GetNativeDescriptoPool() const
	{
		return mDescriptorPool;
	}

	void DescriptorPool::AllocDescriptorSet(
		const std::unique_ptr<Device>& device, 
		const std::unique_ptr<DescriptorSetLayout>& descriptorSetLayout,
		VkDescriptorSet* descriptorSets
	) const
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = mDescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = descriptorSetLayout->GetNativeLayout();

		if (vkAllocateDescriptorSets(device->GetNativeDevice(), &allocInfo, descriptorSets) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate descriptor sets!");
		}
	}

	void DescriptorPool::FreeDescriptorSet(
		const std::unique_ptr<Device>& device,
		VkDescriptorSet* pDescriptorSet) const
	{
		vkFreeDescriptorSets(device->GetNativeDevice(), mDescriptorPool, 1, pDescriptorSet);
	}

}




