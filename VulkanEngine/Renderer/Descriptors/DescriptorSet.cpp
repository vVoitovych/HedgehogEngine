#include "DescriptorSet.h"
#include "VulkanEngine/Renderer/Device/Device.h"
#include "DescriptorPool.h"
#include "DescriptorSetLayout.h"
#include "UBOInfo.h"
#include "UBO.h"
#include "VulkanEngine/Logger/Logger.h"

namespace Renderer
{
	DescriptorSet::DescriptorSet()
		: mDevice(nullptr)
        , mDescriptorPool(nullptr)
		, mDescriptorSet(nullptr)
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

	void DescriptorSet::Initialize(Device& device, DescriptorPool& descriptorPool, DescriptorSetLayout& descriptorSetLayout, UBO& ubo)
	{
        mDevice = device.GetNativeDevice();
        mDescriptorPool = descriptorPool.GetNativePool();

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool.GetNativePool();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = descriptorSetLayout.GetNativeLayout();

        if (vkAllocateDescriptorSets(device.GetNativeDevice(), &allocInfo, &mDescriptorSet) != VK_SUCCESS) 
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

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

	void DescriptorSet::Cleanup()
	{
        vkFreeDescriptorSets(mDevice, mDescriptorPool, 1, &mDescriptorSet);
        mDescriptorSet = nullptr;
	}

    VkDescriptorSet* DescriptorSet::GetNativeSet()
    {
        return &mDescriptorSet;
    }

}



