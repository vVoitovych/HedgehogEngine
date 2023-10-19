#include "DescriptorSet.h"
#include "VulkanEngine/Renderer/Device/Device.h"
#include "DescriptorSetLayout.h"
#include "UBOInfo.h"
#include "UBO.h"
#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.h"

namespace Renderer
{
	DescriptorSet::DescriptorSet()
		: mDescriptorSet(nullptr)
	{
	}

	DescriptorSet::~DescriptorSet()
	{
        if (mDescriptorSet != nullptr)
        {
            LOGERROR("Vulkan description set should be cleanedup before destruction!");
            ENGINE_DEBUG_BREAK();
        }
	}

	void DescriptorSet::Initialize(const Device& device, DescriptorSetLayout& descriptorSetLayout, UBO& ubo)
	{
        device.AllocateDescriptorSet(descriptorSetLayout, ubo, &mDescriptorSet);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = ubo.GetNativeBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = mDescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;

        vkUpdateDescriptorSets(device.GetNativeDevice(), 1, &descriptorWrite, 0, nullptr);
        LOGINFO("Vulkan descriptor set created");
	}

	void DescriptorSet::Cleanup(const Device& device)
	{
        device.FreeDescriptorSet(&mDescriptorSet);
        mDescriptorSet = nullptr;
        LOGINFO("Vulkan descriptor set cleaned");
	}

    VkDescriptorSet* DescriptorSet::GetNativeSet()
    {
        return &mDescriptorSet;
    }

}



