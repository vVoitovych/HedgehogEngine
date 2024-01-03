#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;
	class RenderContext;

	class UBO
	{
	public:
		UBO(const std::unique_ptr<Device>& device);
		~UBO();

		UBO(const UBO&) = delete;
		UBO& operator=(const UBO&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);

		void UpdateUniformBuffer(std::unique_ptr< RenderContext>& context);
		VkBuffer GetNativeBuffer();
	private:
		VkBuffer mUniformBuffer;
		VkDeviceMemory mUniformBufferMemory;
		void* mUniformBufferMapped;
	};
}

