#include "Device.h"
#include "CommonApiData.h"

namespace Renderer
{
	Device::Device()
		: mPhysicalDevice(VK_NULL_HANDLE)
		, mDevice(VK_NULL_HANDLE)
	{
	}

	Device::~Device()
	{
		if (mDevice != nullptr)
		{
			throw std::runtime_error("Vulkan device should be cleanedup before destruction!");
		}
	}

	void Device::Initialize(Instance& instance, Surface& surface)
	{
		PickPhysicalDevice(instance, surface);
		CreateLogicalDevice(instance, surface);
	}

	void Device::Cleanup()
	{
		vkDestroyDevice(mDevice, nullptr);
		mDevice = nullptr;
	}

	VkQueue Device::GetGraphicsQueue() const
	{
		return mGraphicsQueue;
	}

	VkQueue Device::GetPresentQueue() const
	{
		return mPresentQueue;
	}

	VkDevice Device::GetDevice() const
	{
		return mDevice;
	}

	VkPhysicalDevice Device::GetPhysicalDevice() const
	{
		return mPhysicalDevice;
	}

	QueueFamilyIndices Device::GetIndicies() const
	{
		return mIndices;
	}

	uint32_t Device::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
	}

	void Device::PickPhysicalDevice(Instance& instance, Surface& surface)
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance.GetInstance(), &deviceCount, nullptr);
		std::cout << "Devices count: " << deviceCount << std::endl;
		if (deviceCount == 0)
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance.GetInstance(), &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(device, instance, surface))
			{
				mPhysicalDevice = device;
				break;
			}
		}

		if (mPhysicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
		std::cout << "Physical device: " << properties.deviceName << std::endl;

		std::cout << "Physical device picked" << std::endl;
	}

	void Device::CreateLogicalDevice(Instance& instance, Surface& surface)
	{
		mIndices = FindQueueFamilies(mPhysicalDevice, instance, surface);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { mIndices.mGraphicsFamily.value(), mIndices.mPresentFamily.value() };

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo{};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();

		createInfo.pEnabledFeatures = &deviceFeatures;

		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (instance.IsEnableValidationLayers())
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device!");
		}

		vkGetDeviceQueue(mDevice, mIndices.mGraphicsFamily.value(), 0, &mGraphicsQueue);
		vkGetDeviceQueue(mDevice, mIndices.mPresentFamily.value(), 0, &mPresentQueue);

		std::cout << "Logical device created" << std::endl;
	}


	bool Device::CheckDeviceExtensionSupport(VkPhysicalDevice device, Instance& instance) const
	{
		uint32_t extensionsCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionsCount); 
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, availableExtensions.data());

		std::set<std::string> requiredExtensins(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensins.erase(extension.extensionName);
		}

		return requiredExtensins.empty();
	}

	bool Device::IsDeviceSuitable(VkPhysicalDevice device, Instance& instance, Surface& surface) const
	{
		QueueFamilyIndices indices = FindQueueFamilies(device, instance, surface);
		bool extensionsSupported = CheckDeviceExtensionSupport(device, instance);
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			auto swapChainSupport = QuerySwapChainSupport(device, surface);
			swapChainAdequate = !swapChainSupport.mFormats.empty() && !swapChainSupport.mPrecentModes.empty();
		}
		return indices.IsComplete() && extensionsSupported && swapChainAdequate;
	}

	QueueFamilyIndices Device::FindQueueFamilies(VkPhysicalDevice device, Instance& instance, Surface& surface) const
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.mGraphicsFamily = i;
			}
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface.GetSurface(), &presentSupport);

			if (presentSupport)
			{
				indices.mPresentFamily = i;
			}

			if (indices.IsComplete())
			{
				break;
			}
			i++;
		}
		return indices;
	}

	SwapChainSupportDetails Device::QuerySwapChainSupport(VkPhysicalDevice device, Surface& surface) const
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface.GetSurface(), &details.mCapabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface.GetSurface(), &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.mFormats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface.GetSurface(), &formatCount, details.mFormats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface.GetSurface(), &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.mPrecentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface.GetSurface(), &presentModeCount, details.mPrecentModes.data());
		}
		return details;
	}

}


