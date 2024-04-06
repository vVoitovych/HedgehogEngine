#pragma once

#include "DescriptorInfo.hpp"

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace Renderer
{
	class Device;

	class DescriptorSetLayout
	{
	public:
		DescriptorSetLayout(
			const std::unique_ptr<Device>& device,
			const std::vector<DescriptorInfo>& bindingUBOs,
			const std::vector<DescriptorInfo>& bindingSamplers);
		~DescriptorSetLayout();

		DescriptorSetLayout(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkDescriptorSetLayout GetNativeLayout();
		VkDescriptorSetLayout* GetNativeLayoutPtr();
	private:
		VkDescriptorSetLayout mDescriptorSetLayout;
	};
}



