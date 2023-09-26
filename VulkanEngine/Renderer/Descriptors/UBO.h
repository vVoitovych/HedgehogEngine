#pragma once

#include "VulkanEngine/Renderer/Common/pch.h"

namespace Renderer
{
	class Device;

	class UBO
	{
	public:
		UBO();
		~UBO();

		UBO(const UBO&) = delete;
		UBO& operator=(const UBO&) = delete;

		void Initialize(Device& device);
		void Cleanup();

		void UpdateUniformBuffer(float time, float ratio);
		VkBuffer GetNativeBuffer();
	private:
		VkDevice mDevice;

		VkBuffer mUniformBuffer;
		VkDeviceMemory mUniformBufferMemory;
		void* mUniformBufferMapped;
	};
}

