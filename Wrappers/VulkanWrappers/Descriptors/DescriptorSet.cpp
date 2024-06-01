#include "DescriptorSet.hpp"
#include "DescriptorSetLayout.hpp"
#include "VulkanWrappers/Device/Device.hpp"
#include "VulkanWrappers/Descriptors/DescriptorAllocator.hpp"
#include "VulkanWrappers/Resources/Buffer/Buffer.hpp"
#include "VulkanWrappers/Resources/Image/Image.hpp"
#include "VulkanWrappers/Resources/Sampler/Sampler.hpp"

#include "Logger/Logger.hpp"
#include "Common/EngineDebugBreak.hpp"

namespace Hedgehog
{
    namespace Wrappers
    {
        DescriptorSet::DescriptorSet(
            const Device & device,
            const DescriptorAllocator & allocator,
            const DescriptorSetLayout & descriptorSetLayout)
            : mDescriptorSet(nullptr)
        {
            mDescriptorSet = allocator.Allocate(device, descriptorSetLayout);
            LOGINFO("Vulkan descriptor set created");
        }

        DescriptorSet::~DescriptorSet()
        {
            if (mDescriptorSet != nullptr)
            {
                LOGERROR("Vulkan description set should be cleanedup before destruction!");
                ENGINE_DEBUG_BREAK();
            }
        }

        DescriptorSet::DescriptorSet(DescriptorSet && other) noexcept
            : mDescriptorSet(other.mDescriptorSet)
        {
            other.mDescriptorSet = nullptr;
        }

        DescriptorSet& DescriptorSet::operator=(DescriptorSet && other) noexcept
        {
            if (this != &other)
            {
                mDescriptorSet = other.mDescriptorSet;

                other.mDescriptorSet = nullptr;
            }
            return *this;
        }

        void DescriptorSet::Update(const Device & device, std::vector<DescriptorWrites>&descriptorWrites)
        {
            std::vector<VkWriteDescriptorSet> writes;
            writes.resize(descriptorWrites.size());
            for (size_t i = 0; i < descriptorWrites.size(); ++i)
            {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = mDescriptorSet;
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

        void DescriptorSet::Cleanup(const Device & device, const DescriptorAllocator & allocator)
        {
            allocator.FreeDescriptorSet(device, &mDescriptorSet);
            mDescriptorSet = nullptr;
            LOGINFO("Vulkan descriptor set cleaned");
        }

        const VkDescriptorSet* DescriptorSet::GetNativeSet() const
        {
            return &mDescriptorSet;
        }

        VkDescriptorSet* DescriptorSet::GetNativeSet()
        {
            return &mDescriptorSet;
        }

    }
}


