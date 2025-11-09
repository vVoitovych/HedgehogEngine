#pragma once

#include <vulkan/vulkan.h>

namespace Wrappers
{
	class Instance;

	class PhysicalDevice
	{
	public:
		PhysicalDevice(const Instance& instance);
		~PhysicalDevice();

		PhysicalDevice(const PhysicalDevice&) = delete;
		PhysicalDevice(PhysicalDevice&&) = delete;
		PhysicalDevice& operator=(const PhysicalDevice&) = delete;
		PhysicalDevice& operator=(PhysicalDevice&&) = delete;

		const VkPhysicalDevice& GetNativeDevice() const;
		uint32_t GetGraphichQueueIndex() const;


	private:
		VkPhysicalDevice m_PhysicalDevice;

	};


}

