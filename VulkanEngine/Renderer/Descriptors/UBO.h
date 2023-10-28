#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	class Device;
	class Camera;

	class UBO
	{
	public:
		UBO();
		~UBO();

		UBO(const UBO&) = delete;
		UBO& operator=(const UBO&) = delete;

		void Initialize(const Device& device);
		void Cleanup(const Device& device);

		void UpdateUniformBuffer(float time, Camera& camera, float ratio);
		VkBuffer GetNativeBuffer();
	private:
		VkBuffer mUniformBuffer;
		VkDeviceMemory mUniformBufferMemory;
		void* mUniformBufferMapped;
	};
}

