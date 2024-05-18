#pragma once

#include "DescriptorInfo.hpp"

#include <vulkan/vulkan.h>
#include <vector>

namespace Renderer
{
	class Device;

	class DescriptorSetLayout
	{
	public:
		DescriptorSetLayout(
			const Device& device,
			const std::vector<DescriptorInfo>& bindingUBOs,
			const std::vector<DescriptorInfo>& bindingSamplers);
		~DescriptorSetLayout();

		DescriptorSetLayout(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

		void Cleanup(const Device& device);

		VkDescriptorSetLayout GetNativeLayout() const;
		VkDescriptorSetLayout* GetNativeLayoutPtr();
	private:
		VkDescriptorSetLayout mDescriptorSetLayout;
	};
}



