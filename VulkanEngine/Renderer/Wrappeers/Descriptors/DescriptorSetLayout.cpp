#include "DescriptorSetLayout.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"

#include <array>

namespace Renderer
{
	DescriptorSetLayout::DescriptorSetLayout(const std::unique_ptr<Device>& device)
		: mDescriptorSetLayout(nullptr)
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(device->GetNativeDevice(), &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
		LOGINFO("Descriptor set layout initialized");
	}

	DescriptorSetLayout::~DescriptorSetLayout()
	{
		if (mDescriptorSetLayout != nullptr)
		{
			LOGERROR("Vulkan description set layout should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void DescriptorSetLayout::Cleanup(const std::unique_ptr<Device>& device)
	{
		vkDestroyDescriptorSetLayout(device->GetNativeDevice(), mDescriptorSetLayout, nullptr);
		mDescriptorSetLayout = nullptr;
		LOGINFO("Descriptor set layout cleaned");
	}
	VkDescriptorSetLayout DescriptorSetLayout::GetNativeLayout() 
	{
		return mDescriptorSetLayout;
	}
	VkDescriptorSetLayout* DescriptorSetLayout::GetNativeLayoutPtr()
	{
		return &mDescriptorSetLayout;
	}
}

