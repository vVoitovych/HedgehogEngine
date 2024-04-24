#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;
	class RenderContext;
	class Buffer;

	class UBO
	{
	public:
		UBO(const std::unique_ptr<Device>& device);
		~UBO();

		UBO(const UBO&) = delete;
		UBO& operator=(const UBO&) = delete;

		UBO(UBO&& other) noexcept;
		UBO& operator=(UBO&& other) noexcept;

		void Cleanup(const std::unique_ptr<Device>& device);

		void UpdateUniformBuffer(std::unique_ptr< RenderContext>& context);
		VkBuffer GetNativeBuffer();
		const std::unique_ptr<Buffer>& GetBuffer() const;

	private:
		std::unique_ptr<Buffer> mUniformBuffer;

		void* mUniformBufferMapped;
	};
}

