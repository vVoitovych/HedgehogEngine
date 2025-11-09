#include "Instance.hpp"

#ifdef DEBUG
#include "DebugCallback.hpp"
#endif

#define GLFW_INCLUDE_VULKAN
#include "ThirdParty/glfw/glfw/include/GLFW/glfw3.h"

#include <cassert>
#include <string>
#include <unordered_set>

namespace Wrappers
{
	Instance::Instance()
		: m_Instance(nullptr)
	{
		assert(CheckLayersSupport());

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Vulkan Engine";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_3;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		auto extensions = GetRequiredExtensions();
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		createInfo.enabledLayerCount = static_cast<uint32_t>(m_Layers.size());
		createInfo.ppEnabledLayerNames = m_Layers.data();

#ifdef DEBUG
		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
		createInfo.enabledLayerCount = static_cast<uint32_t>(m_Layers.size());
		createInfo.ppEnabledLayerNames = m_Layers.data();

		m_DebugMessangerInfo = {};
		m_DebugMessangerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		m_DebugMessangerInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		m_DebugMessangerInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		m_DebugMessangerInfo.pfnUserCallback = debugCallback;
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&m_DebugMessangerInfo;
#else
		createInfo.enabledLayerCount = 0;
		createInfo.pNext = nullptr;
#endif

		assert(vkCreateInstance(&createInfo, nullptr, &m_Instance) == VK_SUCCESS);

		CheckInstanceExtensions();
	}

	Instance::~Instance()
	{
		assert(m_Instance == nullptr);
	}

	void Instance::Cleanup()
	{
		vkDestroyInstance(m_Instance, nullptr);
		m_Instance = nullptr;
	}

	VkInstance Instance::GetNativeInstance()
	{
		return m_Instance;
	}

	const VkInstance Instance::GetNativeInstance() const
	{
		return m_Instance;
	}

	const std::vector<const char*>& Instance::GetLayers() const
	{
		return m_Layers;
	}

#ifdef DEBUG
	VkDebugUtilsMessengerCreateInfoEXT Instance::GetDebugMessangerInfo() const
	{
		return m_DebugMessangerInfo;
	}
#endif

	void Instance::InitializeLayers()
	{
		m_Layers.clear();
#ifdef DEBUG
		m_Layers.push_back("VK_LAYER_KHRONOS_validation");
#endif
	}

	bool Instance::CheckLayersSupport() const
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : m_Layers)
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

	std::vector<const char*> Instance::GetRequiredExtensions() const
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
#ifdef DEBUG
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
		return extensions;
	}

	void Instance::CheckInstanceExtensions() const
	{
		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

		std::unordered_set<std::string> available;
		for (const auto& extension : extensions)
		{
			available.insert(extension.extensionName);
		}

		auto requiredExtensions = GetRequiredExtensions();
		for (const auto& required : requiredExtensions)
		{
			assert(available.find(required) != available.end());			
		}
	}

}


