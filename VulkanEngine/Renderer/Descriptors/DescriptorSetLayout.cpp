#include "DescriptorSetLayout.h"
#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.h"

namespace Renderer
{
	DescriptorSetLayout::DescriptorSetLayout()
		: mDescriptorSetLayout(nullptr)
	{
	}

	DescriptorSetLayout::~DescriptorSetLayout()
	{
		if (mDescriptorSetLayout != nullptr)
		{
			LOGERROR("Vulkan description set layout should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void DescriptorSetLayout::Initialize(const Device& device)
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &uboLayoutBinding;

		if (vkCreateDescriptorSetLayout(device.GetNativeDevice(), &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
	}

	void DescriptorSetLayout::Cleanup(const Device& device)
	{
		vkDestroyDescriptorSetLayout(device.GetNativeDevice(), mDescriptorSetLayout, nullptr);
		mDescriptorSetLayout = nullptr;
		LOGINFO("Descriptor set layout cleaned");
	}
	VkDescriptorSetLayout* DescriptorSetLayout::GetNativeLayout() 
	{
		return &mDescriptorSetLayout;
	}
}

