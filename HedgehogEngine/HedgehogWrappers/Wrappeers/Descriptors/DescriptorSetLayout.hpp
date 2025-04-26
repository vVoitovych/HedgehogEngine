#pragma once

#include "DescriptorInfo.hpp"

#include <vulkan/vulkan.h>
#include <vector>

namespace Wrappers
{
	class Device;
	class DescriptorLayoutBuilder;

	class DescriptorSetLayout
	{
	public:
		DescriptorSetLayout(
			const Device& device, 
			DescriptorLayoutBuilder& builder, 
			VkShaderStageFlags shaderStages, 
			void* pNext = nullptr, 
			VkDescriptorSetLayoutCreateFlags flags = 0);
		~DescriptorSetLayout();

		DescriptorSetLayout(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

		DescriptorSetLayout(DescriptorSetLayout&& rhs) noexcept;
		DescriptorSetLayout& operator=(DescriptorSetLayout&& rhs) noexcept;

		void Cleanup(const Device& device);

		VkDescriptorSetLayout GetNativeLayout() const;
		const VkDescriptorSetLayout* GetNativeLayoutPtr() const;
	private:
		VkDescriptorSetLayout m_DescriptorSetLayout;
	};
}



