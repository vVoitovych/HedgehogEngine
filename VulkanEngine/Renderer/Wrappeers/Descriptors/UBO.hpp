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
		UBO(const Device& device);
		~UBO();

		UBO(const UBO&) = delete;
		UBO& operator=(const UBO&) = delete;

		UBO(UBO&& other) noexcept;
		UBO& operator=(UBO&& other) noexcept;

		void Cleanup(const Device& device);

		void UpdateUniformBuffer(const RenderContext& context);
		VkBuffer GetNativeBuffer();
		const Buffer& GetBuffer() const;

	private:
		std::unique_ptr<Buffer> mUniformBuffer;

		void* mUniformBufferMapped;
	};
}

