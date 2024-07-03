#include "DescriptorAllocator.hpp"
#include "DescriptorSetLayout.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"

#include <stdexcept>

namespace Renderer
{
	DescriptorAllocator::DescriptorAllocator(const Device& device, uint32_t maxSets, const std::vector<PoolSizeRatio>& poolRatios)
		: mPool(nullptr)
	{
		std::vector<VkDescriptorPoolSize> poolSizes;
		for (PoolSizeRatio ratio : poolRatios)
		{
			VkDescriptorPoolSize poolSize{};
			poolSize.type = ratio.type;
			poolSize.descriptorCount = uint32_t(ratio.ratio * maxSets);
			poolSizes.push_back(poolSize);
		}

		VkDescriptorPoolCreateInfo info{};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		info.maxSets = maxSets;
		info.poolSizeCount = (uint32_t)poolSizes.size();
		info.pPoolSizes = poolSizes.data();

		vkCreateDescriptorPool(device.GetNativeDevice(), &info, nullptr, &mPool);
		LOGINFO("Descriptor allocator created");
	}

	DescriptorAllocator::~DescriptorAllocator()
	{
		if (mPool != nullptr)
		{
			LOGERROR("Vulkan descriptor pool should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void DescriptorAllocator::Cleanup(const Device& device)
	{
		vkDestroyDescriptorPool(device.GetNativeDevice(), mPool, nullptr);
		mPool = nullptr;
	}

	const VkDescriptorPool DescriptorAllocator::GetNativeDescriptoPool() const
	{
		return mPool;
	}

	VkDescriptorPool DescriptorAllocator::GetNativeDescriptoPool()
	{
		return mPool;
	}

	VkDescriptorSet DescriptorAllocator::Allocate(const Device& device, const DescriptorSetLayout& layout) const
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = nullptr;
		allocInfo.descriptorPool = mPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layout.GetNativeLayoutPtr();

		VkDescriptorSet result;
		if (vkAllocateDescriptorSets(device.GetNativeDevice(), &allocInfo, &result) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate descriptor sets!");
		}

		return result;
	}

	void DescriptorAllocator::FreeDescriptorSet(const Device& device, VkDescriptorSet* pDescriptorSet) const
	{
		vkFreeDescriptorSets(device.GetNativeDevice(), mPool, 1, pDescriptorSet);
	}

}



