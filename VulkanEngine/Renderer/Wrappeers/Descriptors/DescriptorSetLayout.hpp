#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;

	class DescriptorSetLayout
	{
	public:
		DescriptorSetLayout(const std::unique_ptr<Device>& device);
		~DescriptorSetLayout();

		DescriptorSetLayout(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkDescriptorSetLayout* GetNativeLayout();
	private:
		VkDescriptorSetLayout mDescriptorSetLayout;
	};
}



