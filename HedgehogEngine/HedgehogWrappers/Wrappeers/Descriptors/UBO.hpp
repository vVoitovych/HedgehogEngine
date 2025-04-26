#pragma once

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/Resources/Buffer/Buffer.hpp"

#include "Logger/Logger.hpp"

#include <vulkan/vulkan.h>
#include <memory>

namespace Wrappers
{
	class Device;
	class RenderContext;
	class Buffer;

	template<typename T>
	class UBO
	{
	public:
		UBO(const Device& device) 
			: m_UniformBufferMapped(nullptr)
		{
			VkDeviceSize bufferSize = sizeof(T);
			m_UniformBuffer = std::make_unique<Buffer>(device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

			m_UniformBuffer->MapMemory(device, &m_UniformBufferMapped);
			LOGINFO("Vulkan UBO created");
		}
		~UBO() {}

		UBO(const UBO&) = delete;
		UBO& operator=(const UBO&) = delete;

		UBO(UBO&& other) noexcept
			: m_UniformBuffer(std::move(other.m_UniformBuffer))
			, m_UniformBufferMapped(other.m_UniformBufferMapped)
		{
			other.m_UniformBufferMapped = nullptr;
		}

		UBO& operator=(UBO&& other) noexcept
		{
			if (this != &other)
			{
				m_UniformBuffer = std::move(other.m_UniformBuffer);
				m_UniformBufferMapped = other.m_UniformBufferMapped;

				other.m_UniformBufferMapped = nullptr;
			}
			return *this;
		}

		void Cleanup(const Device& device)
		{
			m_UniformBuffer->UnapMemory(device);
			m_UniformBuffer->DestroyBuffer(device);
			LOGINFO("UBO cleaned");
		}

		void UpdateUniformBuffer(const T& ubo)
		{
			memcpy(m_UniformBufferMapped, &ubo, sizeof(ubo));
		}

		VkBuffer GetNativeBuffer()
		{
			return m_UniformBuffer->GetNativeBuffer();
		}

		const Buffer& GetBuffer() const
		{
			return *m_UniformBuffer;
		}

		VkDeviceSize GetBufferSize() const
		{
			return m_UniformBuffer->GetBufferSize();
		}

	private:
		std::unique_ptr<Buffer> m_UniformBuffer;

		void* m_UniformBufferMapped;
	};
}

