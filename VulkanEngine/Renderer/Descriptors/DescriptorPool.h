#pragma once
#include "VulkanEngine/Renderer/Common/pch.h"

namespace Renderer
{
	class Device;

	class DescriptorPool
	{
	public:
		DescriptorPool();
		~DescriptorPool();

		DescriptorPool(const DescriptorPool&) = delete;
		DescriptorPool& operator=(const DescriptorPool&) = delete;

		void Initialize(Device& device);
		void Cleanup();

		VkDescriptorPool GetNativePool();
	private:
		VkDevice mDevice;
		VkDescriptorPool mDescriptorPool;
	};

}



