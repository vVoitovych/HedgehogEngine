#include "DescriptorSetLayout.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"

#include <array>

namespace Renderer
{
	DescriptorSetLayout::DescriptorSetLayout(
		const Device& device,
		const std::vector<DescriptorInfo>& bindingUBOs,
		const std::vector<DescriptorInfo>& bindingSamplers
	)
		: mDescriptorSetLayout(nullptr)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.clear();
		for (size_t i = 0; i < bindingUBOs.size(); ++i)
		{
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = bindingUBOs[i].bindingNumber;
			binding.descriptorType = bindingUBOs[i].descriptorType;
			binding.descriptorCount = 1;
			binding.stageFlags = bindingUBOs[i].shaderStage;
			binding.pImmutableSamplers = nullptr;

			bindings.push_back(binding);
		}
		for (size_t i = 0; i < bindingUBOs.size(); ++i)
		{
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = bindingSamplers[i].bindingNumber;
			binding.descriptorType = bindingSamplers[i].descriptorType;
			binding.descriptorCount = 1;
			binding.stageFlags = bindingSamplers[i].shaderStage;
			binding.pImmutableSamplers = nullptr;

			bindings.push_back(binding);
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(device.GetNativeDevice(), &layoutInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
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

	void DescriptorSetLayout::Cleanup(const Device& device)
	{
		vkDestroyDescriptorSetLayout(device.GetNativeDevice(), mDescriptorSetLayout, nullptr);
		mDescriptorSetLayout = nullptr;
		LOGINFO("Descriptor set layout cleaned");
	}
	VkDescriptorSetLayout DescriptorSetLayout::GetNativeLayout() const
	{
		return mDescriptorSetLayout;
	}
	VkDescriptorSetLayout* DescriptorSetLayout::GetNativeLayoutPtr()
	{
		return &mDescriptorSetLayout;
	}
}

