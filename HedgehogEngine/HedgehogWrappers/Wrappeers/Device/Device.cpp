#include "Device.hpp"
#include "Instance.hpp"
#include "DebugCallback.hpp"
#include "DebugMessanger.hpp"

#include "HedgehogWrappers/WindowManagment/WindowManager.hpp"
#include "HedgehogWrappers/Wrappeers/Commands/CommandBuffer.hpp"

#include "HedgehogCommon/Common/EngineDebugBreak.hpp"
#include "HedgehogCommon/Common/RendererSettings.hpp"

#include "Logger/Logger.hpp"

#define VMA_IMPLEMENTATION
#include "vma/vk_mem_alloc.h"

#define GLFW_INCLUDE_VULKAN
#include "ThirdParty/glfw/glfw/include/GLFW/glfw3.h"

#include <vector>
#include <set>
#include <unordered_set>
#include <array>

namespace Wrappers
{
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
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
				indices.graphicsFamily = i;
			}
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.IsComplete())
			{
				break;
			}
			i++;
		}
		return indices;
	}

	SwapChainSupportDetails GetSwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
		if (presentModeCount != 0)
		{
			details.precentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.precentModes.data());
		}
		return details;
	}

	// Device methods

	Device::Device(const WinManager::WindowManager& windowManager)
		: m_Instance(nullptr)
		, m_DebugMessenger(nullptr)
		, m_Surface(nullptr)
		, m_PhysicalDevice(nullptr)
		, m_Device(nullptr)
		, m_GraphicsQueue(nullptr)
		, m_PresentQueue(nullptr)
	{
		InitLayersAndExtentions();
		m_Instance = std::make_unique<Instance>();
		m_DebugMessenger = std::make_unique<DebugMessager>(*m_Instance);
		windowManager.CreateWindowSurface(m_Instance->GetNativeInstance(), &m_Surface);
		PickPhysicalDevice();
		CreateLogicalDevice();
		InitializeAllocator();
		InitializeCommandPool();
	}

	Device::~Device()
	{
		if (m_PhysicalDevice != nullptr)
		{
			LOGERROR("Vulkan physical device should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (m_Device != nullptr)
		{
			LOGERROR("Vulkan device should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (m_Allocator != nullptr)
		{
			LOGERROR("Vulkan allocator should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (m_CommandPool != nullptr)
		{
			LOGERROR("Vulkan command pool should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void Device::InitLayersAndExtentions()
	{
		m_DeviceExtensions.clear();

		m_DeviceExtensions.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
		m_DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	void Device::PickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_Instance->GetNativeInstance(), &deviceCount, nullptr);
		LOGINFO("Devices count: ", deviceCount);
		if (deviceCount == 0)
		{
			throw std::runtime_error("failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_Instance->GetNativeInstance(), &deviceCount, devices.data());
		
		int maxScore = 0;
		for (const auto& device : devices)
		{
			int score = GetDeviceScore(device);
			if (score > maxScore)
			{
				m_PhysicalDevice = device;
				maxScore = score;
			}
		}

		if (m_PhysicalDevice == nullptr)
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}

		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
		LOGINFO("Physical device: ", properties.deviceName);

		LOGINFO("Physical device picked");
	}

	void Device::CreateLogicalDevice()
	{
		m_Indices = FindQueueFamilies(m_PhysicalDevice, m_Surface);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = { m_Indices.graphicsFamily.value(), m_Indices.presentFamily.value() };

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

		VkPhysicalDeviceSynchronization2FeaturesKHR synchronization2Features = {};
		synchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;
		synchronization2Features.synchronization2 = VK_TRUE;


		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
		createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

#ifdef DEBUG
		createInfo.enabledLayerCount = static_cast<uint32_t>(m_Instance->GetLayers().size());
		createInfo.ppEnabledLayerNames = m_Instance->GetLayers().data();
#else
		createInfo.enabledLayerCount = 0;
#endif
		createInfo.pNext = &synchronization2Features;

		if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device!");
		}

		vkGetDeviceQueue(m_Device, m_Indices.graphicsFamily.value(), 0, &m_GraphicsQueue);
		vkGetDeviceQueue(m_Device, m_Indices.presentFamily.value(), 0, &m_PresentQueue);

		LOGINFO("Logical device created");
	}

	void Device::InitializeAllocator()
	{
		VmaVulkanFunctions vulkanFunctions = {};
		vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

		VmaAllocatorCreateInfo allocatorCreateInfo = {};
		allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
		allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
		allocatorCreateInfo.physicalDevice = m_PhysicalDevice;
		allocatorCreateInfo.device = m_Device;
		allocatorCreateInfo.instance = m_Instance->GetNativeInstance();
		allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

		vmaCreateAllocator(&allocatorCreateInfo, &m_Allocator);
	}

	void Device::InitializeCommandPool()
	{
		VkCommandPoolCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		createInfo.queueFamilyIndex = m_Indices.graphicsFamily.value();

		if (vkCreateCommandPool(m_Device, &createInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool!");
		}

		LOGINFO("Command pool created");
	}

	void Device::Cleanup()
	{
		vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
		vmaDestroyAllocator(m_Allocator);
		vkDestroyDevice(m_Device, nullptr);
		vkDestroySurfaceKHR(m_Instance->GetNativeInstance(), m_Surface, nullptr);
		m_DebugMessenger->Cleanup(*m_Instance);
		m_Instance->Cleanup();

		m_PhysicalDevice = nullptr;
		m_Device = nullptr;
		m_Allocator = nullptr;
		m_CommandPool = nullptr;
		LOGINFO("Device cleaned");
	}


	////////////////////////////////////////////////////// 

	VkInstance Device::GetNativeInstance() const
	{
		return m_Instance->GetNativeInstance();
	}

	VkQueue Device::GetNativeGraphicsQueue() const
	{
		return m_GraphicsQueue;
	}

	VkQueue Device::GetNativePresentQueue() const
	{
		return m_PresentQueue;
	}

	VkSurfaceKHR Device::GetNativeSurface() const 
	{
		return m_Surface;
	}

	VkDevice Device::GetNativeDevice() const
	{
		return m_Device;
	}

	VkPhysicalDevice Device::GetNativePhysicalDevice() const
	{
		return m_PhysicalDevice;
	}

	QueueFamilyIndices Device::GetIndicies() const
	{
		return m_Indices;
	}

	const VmaAllocator& Device::GetAllocator() const
	{
		return m_Allocator;
	}

	uint32_t Device::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
	{
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}
		throw std::runtime_error("Failed to find memory type");
	}

	void Device::SetObjectName(uint64_t objectHandle, VkObjectType objectType, const char* name) const
	{
#ifdef DEBUG
		PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT =
			(PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(m_Instance->GetNativeInstance(), "vkSetDebugUtilsObjectNameEXT");

		if (!vkSetDebugUtilsObjectNameEXT) {
			throw std::runtime_error("failed to load vkSetDebugUtilsObjectNameEXT");
		}

		VkDebugUtilsObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
		nameInfo.pNext = nullptr;
		nameInfo.objectType = objectType;
		nameInfo.objectHandle = objectHandle;
		nameInfo.pObjectName = name;

		vkSetDebugUtilsObjectNameEXT(m_Device, &nameInfo);
#endif
	}

	// Additional functions

	SwapChainSupportDetails Device::QuerySwapChainSupport() const
	{
		return GetSwapChainSupport(m_PhysicalDevice, m_Surface);
	}

	VkPhysicalDeviceProperties Device::GetPhysicalDeviceProperties() const
	{
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
		return properties;
	}

	VkFormat Device::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const
	{
		for (VkFormat format : candidates) 
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) 
			{
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) 
			{
				return format;
			}
		}

		throw std::runtime_error("failed to find supported format!");
	}

	VkFormat Device::FindDepthFormat() const
	{
		return FindSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}

	void Device::AllocateCommandBuffer(VkCommandBuffer* pCommandBuffer) const
	{
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.commandPool = m_CommandPool;
		allocateInfo.commandBufferCount = 1;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

		if (vkAllocateCommandBuffers(m_Device, &allocateInfo, pCommandBuffer) != VK_SUCCESS)
		{
			LOGERROR("failed to allocate command buffer!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void Device::FreeCommandBuffer(VkCommandBuffer* pCommandBuffer) const
	{
		vkFreeCommandBuffers(m_Device, m_CommandPool, 1, pCommandBuffer);
	}

	void Device::CopyBufferToBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const
	{
		CommandBuffer commandBuffer(*this);
		commandBuffer.BeginSingleTimeCommands();
		commandBuffer.CopyBufferToBuffer(srcBuffer, dstBuffer, size);
		commandBuffer.EndSingleTimeCommands(*this);
		commandBuffer.Cleanup(*this);
	}

	void Device::CopyBufferToImage(VkBuffer srcBuffer, VkImage image, uint32_t width, uint32_t height) const
	{
		CommandBuffer commandBuffer(*this);
		commandBuffer.BeginSingleTimeCommands();
		commandBuffer.CopyBufferToImage(srcBuffer, image, width, height);
		commandBuffer.EndSingleTimeCommands(*this);
		commandBuffer.Cleanup(*this);
	}

	void Device::TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) const
	{
		CommandBuffer commandBuffer(*this);
		commandBuffer.BeginSingleTimeCommands();
		commandBuffer.TransitionImage(image, oldLayout, newLayout);
		commandBuffer.EndSingleTimeCommands(*this);
		commandBuffer.Cleanup(*this);
	}

	void Device::UpdateDescriptorSets(std::vector<VkWriteDescriptorSet>& descriptorWrites) const
	{
		vkUpdateDescriptorSets(m_Device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

	bool Device::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
	{
		uint32_t extensionsCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionsCount); 
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionsCount, availableExtensions.data());

		std::set<std::string> requiredExtensins(m_DeviceExtensions.begin(), m_DeviceExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensins.erase(extension.extensionName);
		}

		return requiredExtensins.empty();
	}

	int Device::GetDeviceScore(VkPhysicalDevice device) const
	{
		QueueFamilyIndices indices = FindQueueFamilies(device, m_Surface);
		bool extensionsSupported = CheckDeviceExtensionSupport(device);
		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			auto swapChainSupport = GetSwapChainSupport(device, m_Surface);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.precentModes.empty();
		}
		int score = 1;

		bool conditions = indices.IsComplete() && extensionsSupported && swapChainAdequate;
		if (!conditions)
			return 0;

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) 
		{
			score += 1000;
		}

		return score;
	}


}


