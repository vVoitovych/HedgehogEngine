#include "DescriptorLayoutBuilder.hpp"

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"

#include <stdexcept>

namespace Wrappers
{
	void DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type)
	{
		VkDescriptorSetLayoutBinding newBinding{};
		newBinding.binding = binding;
		newBinding.descriptorCount = 1;
		newBinding.descriptorType = type;

		m_Bindings.push_back(newBinding);
	}

	void DescriptorLayoutBuilder::ClearBindings()
	{
		m_Bindings.clear();
	}

	VkDescriptorSetLayout DescriptorLayoutBuilder::Build(const Device& device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
	{
		for (auto& b : m_Bindings) 
		{
			b.stageFlags |= shaderStages;
		}

		VkDescriptorSetLayoutCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		info.pNext = pNext;

		info.pBindings = m_Bindings.data();
		info.bindingCount = (uint32_t)m_Bindings.size();
		info.flags = flags;

		VkDescriptorSetLayout result;
		if (vkCreateDescriptorSetLayout(device.GetNativeDevice(), &info, nullptr, &result) != VK_SUCCESS) {
			throw std::runtime_error("failed to create descriptor set layout!");
		}
		return result;
	}
}


