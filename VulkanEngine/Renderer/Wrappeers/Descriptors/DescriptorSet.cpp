#include "DescriptorSet.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Descriptors/DescriptorPool.hpp"
#include "DescriptorSetLayout.hpp"
#include "Renderer/Wrappeers/Resources/Buffer/Buffer.hpp"

#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Renderer/Wrappeers/Resources/Image/Image.hpp"
#include "Renderer/Wrappeers/Resources/Sampler/Sampler.hpp"

#include <array>

namespace Renderer
{
	DescriptorSet::DescriptorSet(
        const std::unique_ptr<Device>& device,
        const std::unique_ptr<DescriptorPool>& descriptorPool,
        const std::unique_ptr<DescriptorSetLayout>& descriptorSetLayout,
        const std::unique_ptr<Buffer>& ubo,
        const Image& image,
        const Sampler& sampler)
		: mDescriptorSet(nullptr)
	{
        descriptorPool->AllocDescriptorSets(device, { descriptorSetLayout->GetNativeLayout() }, &mDescriptorSet);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = ubo->GetNativeBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = ubo->GetBufferSize();

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = image.GetNativeView();
        imageInfo.sampler = sampler.GetNativeSampler();

        std::array< VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = mDescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = mDescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device->GetNativeDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
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

    DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept
        : mDescriptorSet(other.mDescriptorSet)
    {
        other.mDescriptorSet = nullptr;
    }

    DescriptorSet& DescriptorSet::operator=(DescriptorSet&& other) noexcept
    {
        if (this != &other)
        {
            mDescriptorSet = other.mDescriptorSet;

            other.mDescriptorSet = nullptr;
        }
        return *this;
    }

    void DescriptorSet::Cleanup(const std::unique_ptr<Device>& device, const std::unique_ptr<DescriptorPool>& descriptorPool)
	{
        descriptorPool->FreeDescriptorSet(device, &mDescriptorSet);
        mDescriptorSet = nullptr;
        LOGINFO("Vulkan descriptor set cleaned");
	}

    VkDescriptorSet* DescriptorSet::GetNativeSet()
    {
        return &mDescriptorSet;
    }

}



