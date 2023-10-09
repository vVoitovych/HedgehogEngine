#include "Device.h"
#include "VulkanEngine/Renderer/WindowManagment/WindowManager.h"
#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.h"

#include <vector>
#include <set>
#include <unordered_set>

namespace Renderer
{
	const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
	{
		switch (messageSeverity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			LOGVERBOSE("validation layer: ", pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			LOGINFO("validation layer: ", pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			LOGWARNING("validation layer: ", pCallbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			LOGERROR("validation layer: ", pCallbackData->pMessage);
			break;
		default:
			break;
		}

		return VK_FALSE;
	}

	VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(instance, debugMessenger, pAllocator);
		}
	}

	Device::Device()
		: mInstance(nullptr)
		, mDebugMessenger(nullptr)
		, mSurface(nullptr)
		, mPhysicalDevice(nullptr)
		, mDevice(nullptr)
		, mGraphicsQueue(nullptr)
		, mPresentQueue(nullptr)
	{
	}

	Device::~Device()
	{
		if (mInstance != nullptr)
		{
			LOGERROR("Vulkan instance should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mDebugMessenger != nullptr)
		{
			LOGERROR("Vulkan debug messenger should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mPhysicalDevice != nullptr)
		{
			LOGERROR("Vulkan physical device should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mDevice != nullptr)
		{
			LOGERROR("Vulkan device should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void Device::Initialize(WindowManager& windowManager)
	{
		InitializeInstance();
		InitializeDebugMessanger();
		windowManager.CreateWindowSurface(mInstance, &mSurface);
		PickPhysicalDevice();
		CreateLogicalDevice();
	}

	void Device::InitializeInstance()
	{
		if (enableValidationLayers && !CheckValidationLayerSupport())
		{
			throw std::runtime_error("Validation layers requested, but not available!");
		}

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan Engine";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
			PopulateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
		}
		else
		{
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
		}

		if (vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create instance!");
		}

		HasGflwRequiredInstanceExtensions();

		LOGINFO("Vulkan instance created");
	}

	void Device::InitializeDebugMessanger()
	{
		if (!enableValidationLayers) return;

		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		PopulateDebugMessengerCreateInfo(createInfo);

		if (CreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to set up debug messenger!");
		}
		LOGINFO("Debug messenger created");
	}

	void Device::PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
		LOGINFO("Devices count: ", deviceCount);
		if (deviceCount == 0)
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (IsDeviceSuitable(device))
			{
				mPhysicalDevice = device;
				break;
			}
		}

		if (mPhysicalDevice == nullptr)
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(mPhysicalDevice, &properties);
		LOGINFO("Physical device: ", properties.deviceName);

		LOGINFO("Physical device picked");
	}

	void Device::CreateLogicalDevice()
	{
		mIndices = FindQueueFamilies(mPhysicalDevice);

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

		if (enableValidationLayers)
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

		LOGINFO("Logical device created");
	}

	void Device::Cleanup()
	{
		vkDestroyDevice(mDevice, nullptr);
		vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
		CleanupDebugMessanger();
		vkDestroyInstance(mInstance, nullptr);

		mInstance = nullptr;
		mDebugMessenger = nullptr;
		mPhysicalDevice = nullptr;
		mDevice = nullptr;
		LOGINFO("Device cleaned");
	}

	void Device::CleanupDebugMessanger()
	{
		if (enableValidationLayers)
		{
			DestroyDebugUtilsMessengerEXT(mInstance, mDebugMessenger, nullptr);
		}
	}

	////////////////////////////////////////////////////// 

	VkQueue Device::GetNativeGraphicsQueue() const
	{
		return mGraphicsQueue;
	}

	VkQueue Device::GetNativePresentQueue() const
	{
		return mPresentQueue;
	}

	VkSurfaceKHR Device::GetNativeSurface()
	{
		return mSurface;
	}

	VkDevice Device::GetNativeDevice() const
	{
		return mDevice;
	}

	VkPhysicalDevice Device::GetNativePhysicalDevice() const
	{
		return mPhysicalDevice;
	}

	QueueFamilyIndices Device::GetIndicies() const
	{
		return mIndices;
	}


	bool Device::IsEnableValidationLayers() const
	{
		return enableValidationLayers;
	}

	void Device::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = debugCallback;
	}

	bool Device::CheckValidationLayerSupport() const
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
			{
				return false;
			}
		}
		return true;
	}

	std::vector<const char*> Device::GetRequiredExtensions() const
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		return extensions;
	}

	void Device::HasGflwRequiredInstanceExtensions() const
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		LOGINFO("available extensions:");
		std::unordered_set<std::string> available;
		for (const auto& extension : extensions)
		{
			LOGINFO("\t", extension.extensionName);
			available.insert(extension.extensionName);
		}

		LOGINFO("required extensions:");
		auto requiredExtensions = GetRequiredExtensions();
		for (const auto& required : requiredExtensions)
		{
			LOGINFO("\t", required);
			if (available.find(required) == available.end())
			{
				throw std::runtime_error("Missing required glfw extension");
			}
		}
	}


	bool Device::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
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

	bool Device::IsDeviceSuitable(VkPhysicalDevice device) const
	{
		QueueFamilyIndices indices = FindQueueFamilies(device);
		bool extensionsSupported = CheckDeviceExtensionSupport(device);
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			auto swapChainSupport = QuerySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.mFormats.empty() && !swapChainSupport.mPrecentModes.empty();
		}
		return indices.IsComplete() && extensionsSupported && swapChainAdequate;
	}

	QueueFamilyIndices Device::FindQueueFamilies(VkPhysicalDevice device) const
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
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mSurface, &presentSupport);

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

	SwapChainSupportDetails Device::QuerySwapChainSupport(VkPhysicalDevice device) const
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, mSurface, &details.mCapabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.mFormats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, details.mFormats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.mPrecentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, details.mPrecentModes.data());
		}
		return details;
	}

}


