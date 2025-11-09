#include "PhysicalDevice.hpp"
#include "Instance.hpp"

#include "Logger/Logger.hpp"

#include <cassert>

namespace Wrappers
{
	PhysicalDevice::PhysicalDevice(const Instance& instance)
		: m_PhysicalDevice(nullptr)
 	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance.GetNativeInstance(), &deviceCount, nullptr);
		LOGINFO("Number of GPUs: ", deviceCount);
		assert(deviceCount > 0);

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance.GetNativeInstance(), &deviceCount, devices.data());

		m_PhysicalDevice = devices[0];

		assert(m_PhysicalDevice != nullptr);

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
		LOGINFO("Choosen physical device: ", properties.deviceName);

	}

	PhysicalDevice::~PhysicalDevice()
	{
	}

	const VkPhysicalDevice& PhysicalDevice::GetNativeDevice() const
	{
		return m_PhysicalDevice;
	}

	uint32_t PhysicalDevice::GetGraphichQueueIndex() const
	{
		assert(m_PhysicalDevice != nullptr);
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

		assert(queueFamilyCount > 0);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

		uint32_t i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				return i;
			}

			i++;
		}
		assert(false);
		return -1;
	}

}

