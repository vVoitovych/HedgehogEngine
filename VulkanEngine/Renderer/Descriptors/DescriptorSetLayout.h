#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	class Device;

	class DescriptorSetLayout
	{
	public:
		DescriptorSetLayout();
		~DescriptorSetLayout();

		DescriptorSetLayout(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

		void Initialize(const Device& device);
		void Cleanup(const Device& device);

		VkDescriptorSetLayout* GetNativeLayout();
	private:
		VkDescriptorSetLayout mDescriptorSetLayout;
	};
}



