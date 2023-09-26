#pragma once

#include "VulkanEngine/Renderer/Common/pch.h"

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

		void Initialize(Device& device);
		void Cleanup();

		VkDescriptorSetLayout* GetNativeLayout();
	private:
		VkDevice mDevice;
		VkDescriptorSetLayout mDescriptorSetLayout;
	};
}



