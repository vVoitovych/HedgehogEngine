#pragma once
#include "VulkanEngine/Renderer/Common/pch.h"


namespace Renderer
{
	class Device;
	class DescriptorPool;
	class DescriptorSetLayout;
	class UBO;

	class DescriptorSet
	{
	public:
		DescriptorSet();
		~DescriptorSet();

		DescriptorSet(const DescriptorSet&) = delete;
		DescriptorSet& operator=(const DescriptorSet&) = delete;

		void Initialize(Device& device, DescriptorPool& descriptorPool, DescriptorSetLayout& descriptorSetLayout, UBO& ubo);
		void Cleanup();

		VkDescriptorSet* GetNativeSet();
	private:
		VkDevice mDevice;
		VkDescriptorPool mDescriptorPool;
		VkDescriptorSet mDescriptorSet;

	};


}






