#pragma once

#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/Resources/Buffer/Buffer.hpp"

#include "Logger/Logger.hpp"

#include <vulkan/vulkan.h>
#include <memory>

#include <glm/gtc/matrix_transform.hpp>

namespace Renderer
{
	class Device;
	class RenderContext;
	class Buffer;

	template<typename T>
	class UBO
	{
	public:
		UBO(const Device& device) 
			: mUniformBufferMapped(nullptr)
		{
			VkDeviceSize bufferSize = sizeof(T);
			mUniformBuffer = std::make_unique<Buffer>(device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			mUniformBuffer->MapMemory(device, &mUniformBufferMapped);
			LOGINFO("Vulkan UBO created");
		}
		~UBO() {}

		UBO(const UBO&) = delete;
		UBO& operator=(const UBO&) = delete;

		UBO(UBO&& other) noexcept
			: mUniformBuffer(std::move(other.mUniformBuffer))
			, mUniformBufferMapped(other.mUniformBufferMapped)
		{
			other.mUniformBufferMapped = nullptr;
		}

		UBO& operator=(UBO&& other) noexcept
		{
			if (this != &other)
			{
				mUniformBuffer = std::move(other.mUniformBuffer);
				mUniformBufferMapped = other.mUniformBufferMapped;

				other.mUniformBufferMapped = nullptr;
			}
			return *this;
		}

		void Cleanup(const Device& device)
		{
			mUniformBuffer->UnapMemory(device);
			mUniformBuffer->DestroyBuffer(device);
			LOGINFO("UBO cleaned");
		}

		void UpdateUniformBuffer(const T& ubo)
		{
			memcpy(mUniformBufferMapped, &ubo, sizeof(ubo));
		}

		VkBuffer GetNativeBuffer()
		{
			return mUniformBuffer->GetNativeBuffer();
		}

		const Buffer& GetBuffer() const
		{
			return *mUniformBuffer;
		}

		VkDeviceSize GetBufferSize() const
		{
			return mUniformBuffer->GetBufferSize();
		}

	private:
		std::unique_ptr<Buffer> mUniformBuffer;

		void* mUniformBufferMapped;
	};
}

