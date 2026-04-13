#include "DescriptorSet.hpp"
#include "DescriptorSetLayout.hpp"

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorAllocator.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Image/Image.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Sampler/Sampler.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Buffer/Buffer.hpp"

#include "HedgehogCommon/Common/EngineDebugBreak.hpp"

#include "Logger/Logger.hpp"

namespace Wrappers
{
	DescriptorSet::DescriptorSet(
        const Device& device,
        const DescriptorAllocator& allocator,
        const DescriptorSetLayout& descriptorSetLayout)
		: m_DescriptorSet(nullptr)
	{
        m_DescriptorSet = allocator.Allocate(device, descriptorSetLayout);
        LOGINFO("Vulkan descriptor set created");
	}

	DescriptorSet::~DescriptorSet()
	{
        if (m_DescriptorSet != nullptr)
        {
            LOGERROR("Vulkan description set should be cleanedup before destruction!");
            ENGINE_DEBUG_BREAK();
        }
	}

    DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept
        : m_DescriptorSet(other.m_DescriptorSet)
    {
        other.m_DescriptorSet = nullptr;
    }

    DescriptorSet& DescriptorSet::operator=(DescriptorSet&& other) noexcept
    {
        if (this != &other)
        {
            m_DescriptorSet = other.m_DescriptorSet;

            other.m_DescriptorSet = nullptr;
        }
        return *this;
    }

    void DescriptorSet::Update(const Device& device, std::vector<DescriptorWrites>& descriptorWrites)
    {
        std::vector<VkWriteDescriptorSet> writes;
        writes.resize(descriptorWrites.size());
        for (size_t i = 0; i < descriptorWrites.size(); ++i)
        {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = m_DescriptorSet;
            writes[i].descriptorType = descriptorWrites[i].descriptorType;
            writes[i].dstBinding = descriptorWrites[i].dstBinding;
            writes[i].dstArrayElement = descriptorWrites[i].dstArrayElement;
            writes[i].descriptorCount = descriptorWrites[i].descriptorCount;
            writes[i].pImageInfo = descriptorWrites[i].pImageInfo;
            writes[i].pBufferInfo = descriptorWrites[i].pBufferInfo;
            writes[i].pTexelBufferView = descriptorWrites[i].pTexelBufferView;
            writes[i].pNext = descriptorWrites[i].pNext;
        }

        device.UpdateDescriptorSets(writes);
    }

    void DescriptorSet::Cleanup(const Device& device, const DescriptorAllocator& allocator)
	{        
        allocator.FreeDescriptorSet(device, &m_DescriptorSet);
        m_DescriptorSet = nullptr;
        LOGINFO("Vulkan descriptor set cleaned");
	}

    const VkDescriptorSet* DescriptorSet::GetNativeSet() const
    {
        return &m_DescriptorSet;
    }

    VkDescriptorSet* DescriptorSet::GetNativeSet()
    {
        return &m_DescriptorSet;
    }

}



